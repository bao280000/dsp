/*
 ******************************************************************************
 * 文件名称: c66x_ndk.c
 * 功能描述: TMS320C6678 以太网底层驱动
 *           负责 SGMII SERDES + QMSS + CPPI + PA 硬件子系统初始化
 *           移植自 TL6678F EasyEVM ndk_benchmark 示例（Tronlong）
 * 运行核心: Core 0 Only
 * 内存分布: L2 SRAM
 * 作者    : Zhou
 * 日期    : 2026-03-11
 ******************************************************************************
 */

#include <stdint.h>
#include <stdio.h>

/* CSL */
#include <ti/csl/csl_psc.h>
#include <ti/csl/csl_pscAux.h>
#include <ti/csl/csl_chipAux.h>
#include <ti/csl/csl_bootcfgAux.h>
#include <ti/csl/csl_cpsgmii.h>
#include <ti/csl/csl_cpsgmiiAux.h>

/* Resource Manager (QMSS / CPPI / PA) */
#include "../system/resource_mgr.h"

#include "c66x_ndk.h"

/* NIMUDeviceTable 和 EmacInit 由 nimu_eth.c 源文件提供完整实现；
 * 此处不再重复定义，以确保真实的 EMAC/CPSW/PA/Rx/Tx 完整初始化链路生效。
 */

/* -----------------------------------------------------------------------
 * 内联延迟
 * ---------------------------------------------------------------------- */
static inline void cpu_delaycycles(uint32_t cycles)
{
    uint32_t i;
    for(i = 0; i < cycles; i++) {
        asm(" NOP");
    }
}

/* -----------------------------------------------------------------------
 * SGMII SERDES 初始化
 * @param macPortNum  0 或 1（对应 SGMII Port 0/1）
 * ---------------------------------------------------------------------- */
static int32_t init_sgmii(uint32_t macPortNum)
{
    int32_t             wait_time;
    CSL_SGMII_ADVABILITY sgmiiCfg;
    CSL_SGMII_STATUS    sgmiiStatus;

    /* 配置 SERDES PLL（10x 模式，1 GbE） */
    CSL_BootCfgSetSGMIIConfigPLL(0x00000051);
    cpu_delaycycles(100);

    /* 配置 RX：使能接收通道，Rate 2x，Comma 对齐，EQ 0xC，偏移补偿 */
    CSL_BootCfgSetSGMIIRxConfig(macPortNum, 0x00700621);
    /* 配置 TX：使能发送通道，Rate 2x，正常极性 */
    CSL_BootCfgSetSGMIITxConfig(macPortNum, 0x000108A1);

    /* 等待 SERDES 锁定（最多 1ms，每次延迟 1000 周期） */
    wait_time = 1000;
    while(wait_time) {
        CSL_SGMII_getStatus(macPortNum, &sgmiiStatus);
        if(sgmiiStatus.bIsLocked == 1)
            break;
        cpu_delaycycles(1000);
        wait_time--;
    }

    if(wait_time == 0) {
        printf("[NDK] SGMII Port%u SERDES lock timeout!\n", macPortNum);
        return -1;
    }

    /* 软复位 SGMII 端口 */
    CSL_SGMII_doSoftReset(macPortNum);
    while(CSL_SGMII_getSoftResetStatus(macPortNum) != 0);

    /* 配置 SGMII：禁用 Master 模式，使能自动协商 */
    CSL_SGMII_startRxTxSoftReset(macPortNum);
    CSL_SGMII_disableMasterMode(macPortNum);
    CSL_SGMII_enableAutoNegotiation(macPortNum);
    CSL_SGMII_endRxTxSoftReset(macPortNum);

    /* 广播能力：1G 全双工，链路使能 */
    sgmiiCfg.linkSpeed  = CSL_SGMII_1000_MBPS;
    sgmiiCfg.duplexMode = CSL_SGMII_FULL_DUPLEX;
    sgmiiCfg.bLinkUp    = 1;
    CSL_SGMII_setAdvAbility(macPortNum, &sgmiiCfg);

    /* 等待链路建立 */
    do {
        CSL_SGMII_getStatus(macPortNum, &sgmiiStatus);
    } while(sgmiiStatus.bIsLinkUp != 1);

    printf("[NDK] SGMII Port%u link up (1G Full-duplex)\n", macPortNum);
    return 0;
}

/* -----------------------------------------------------------------------
 * QMSS + CPPI + PA 初始化（仅 Core 0 执行）
 * ---------------------------------------------------------------------- */
static int32_t queue_manager_init(void)
{
    QMSS_CFG_T  qmss_cfg;
    CPPI_CFG_T  cppi_cfg;

    /* --- QMSS --- */
    qmss_cfg.master_core  = (CSL_chipReadDNUM() == 0) ? 1 : 0;
    qmss_cfg.max_num_desc = MAX_NUM_DESC;
    qmss_cfg.desc_size    = MAX_DESC_SIZE;
    qmss_cfg.mem_region   = Qmss_MemRegion_MEMORY_REGION0;

    if(res_mgr_init_qmss(&qmss_cfg) != 0) {
        printf("[NDK] QMSS init failed!\n");
        return -1;
    }
    printf("[NDK] QMSS OK\n");

    /* --- CPPI --- */
    cppi_cfg.master_core    = (CSL_chipReadDNUM() == 0) ? 1 : 0;
    cppi_cfg.dma_num         = Cppi_CpDma_PASS_CPDMA;
    cppi_cfg.num_tx_queues   = NUM_PA_TX_QUEUES;
    cppi_cfg.num_rx_channels = NUM_PA_RX_CHANNELS;

    if(res_mgr_init_cppi(&cppi_cfg) != 0) {
        printf("[NDK] CPPI init failed!\n");
        return -1;
    }
    printf("[NDK] CPPI OK\n");

    /* --- PA (Packet Accelerator) --- */
    if(res_mgr_init_pass() != 0) {
        printf("[NDK] PA init failed!\n");
        return -1;
    }
    printf("[NDK] PA OK\n");

    return 0;
}

/* -----------------------------------------------------------------------
 * 公开接口
 * ---------------------------------------------------------------------- */

/**
 * @brief 初始化以太网硬件子系统（SGMII + QMSS + CPPI + PA）
 */
int32_t ndk_driver_init(void)
{
    /* 解锁芯片级寄存器写保护 */
    CSL_BootCfgUnlockKicker();

    /* 初始化 SGMII SERDES（Port 0 和 Port 1） */
    if(init_sgmii(0) != 0) return -1;
    if(init_sgmii(1) != 0) return -1;

    /* 初始化 QMSS / CPPI / PA （Core 0 主核执行） */
    if(queue_manager_init() != 0) return -1;

    /* 重新上锁 */
    CSL_BootCfgLockKicker();

    return 0;
}

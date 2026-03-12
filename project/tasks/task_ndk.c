/*
 * task_ndk.c
 * TCP/UDP 鍥炵幆娴嬮�熷簲鐢� (闄勫甫 C6678 搴曞眰 PA/QMSS 鍒濆鍖�)
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <ti/ndk/inc/netmain.h>
#include <ti/ndk/inc/_stack.h>
#include <ti/ndk/inc/socket.h>
#include <ti/ndk/inc/socketndk.h>
#include <ti/ndk/inc/stkmain.h>
#include <ti/ndk/inc/os/osif.h>
#include <ti/sysbios/knl/Task.h>
#include "driver/c66x_uart.h"
#include "tasks/task_ndk.h"

/* 寮曞叆璧勬簮绠＄悊鍣紝鐢ㄤ簬鍒濆鍖� PA/QMSS 鍔犻�熷紩鎿� */
#include "system/resource_mgr.h"

/* ============================================================================
 * [NDK 核心内存补丁]
 * 修复 NDK OOM 致命错误：手动提供 NDK 内部对象内存池
 * 覆盖 NDK 底层由于 disableCodeGeneration 而回退的 3KB 极小默认限制
 * ============================================================================ */
#define NDK_MM_SIZE 0x10000  /* 扩充为 64KB，彻底解决 fdOpenSession: OOM */
#pragma DATA_SECTION(NDK_MM_Buffer, ".far:NDK_OBJMEM")
#pragma DATA_ALIGN(NDK_MM_Buffer, 8)
uint8_t NDK_MM_Buffer[NDK_MM_SIZE];

uint8_t* MMBA = NDK_MM_Buffer;
uint32_t MMSize = NDK_MM_SIZE;

/* 闈欐�� IP 閰嶇疆 */
static char *LocalIPAddr = "192.168.1.100";
static char *LocalIPMask = "255.255.255.0";
static char *GatewayIP   = "192.168.1.1";
static char *DomainName  = "demo.net";

static void *hTcpEcho = NULL;
static void *hUdpEcho = NULL;

extern void uart_printf(char *fmt, ...);

int dtask_tcp_echo(SOCKET s, uint32_t unused)
{
    int bytes;
    char *pBuf;
    void *hBuffer;
    (void)unused;

    for(;;)
    {
        bytes = (int)recvnc(s, (void **)&pBuf, 0, &hBuffer);
        if (bytes > 0) {
            send(s, pBuf, bytes, 0);
            recvncfree(hBuffer);
        } else {
            break; 
        }
    }
    return 1;
}

int dtask_udp_echo(SOCKET s, uint32_t unused)
{
    struct sockaddr_in sin1;
    int bytes, tmp;
    char *pBuf;
    void *hBuffer;
    (void)unused;

    for(;;)
    {
        tmp = sizeof(sin1);
        bytes = (int)recvncfrom(s, (void **)&pBuf, 0, (struct sockaddr *)&sin1, &tmp, &hBuffer);
        if (bytes > 0) {
            sendto(s, pBuf, bytes, 0, (struct sockaddr *)&sin1, sizeof(sin1));
            recvncfree(hBuffer);
        }
    }
}

static void NetworkOpen()
{
    printf("[NDK] Network Started. Starting TCP/UDP Echo Benchmark servers on Port 7...\r\n");
    hTcpEcho = DaemonNew(SOCK_STREAMNC, 0, 7, dtask_tcp_echo, OS_TASKPRINORM, 16384, 0, 1);
    hUdpEcho = DaemonNew(SOCK_DGRAM, 0, 7, dtask_udp_echo, OS_TASKPRINORM, 16384, 0, 1);
}

static void NetworkClose()
{
    if (hTcpEcho) { DaemonFree(hTcpEcho); hTcpEcho = NULL; }
    if (hUdpEcho) { DaemonFree(hUdpEcho); hUdpEcho = NULL; }
}

static void NetworkIPAddr(uint32_t IPAddr, uint32_t IfIdx, uint32_t fAdd)
{
    uint32_t IP = NDK_ntohl(IPAddr);
    if(fAdd) {
        printf("[NDK] IP Assigned to If-%d: %d.%d.%d.%d\r\n", IfIdx,
            (uint8_t)(IP>>24)&0xFF, (uint8_t)(IP>>16)&0xFF,
            (uint8_t)(IP>>8)&0xFF, (uint8_t)IP&0xFF);
    } else {
        printf("[NDK] IP Removed from If-%d\r\n", IfIdx);
    }
}

static void ndk_stack_task(uint32_t arg0, uint32_t arg1)
{
    int   rc;
    void *hCfg;
    QMSS_CFG_T qmss_cfg;
    CPPI_CFG_T cppi_cfg;
    (void)arg0;
    (void)arg1;

    /* --- 鏍稿績淇锛氬湪鍚姩 NDK 鍓嶏紝鍒濆鍖� C6678 搴曞眰纭欢寮曟搸 --- */
    /* 1. 鍒濆鍖� QMSS 纭欢闃熷垪 */
    qmss_cfg.master_core = 1;
    qmss_cfg.max_num_desc = MAX_NUM_DESC;
    qmss_cfg.desc_size = MAX_DESC_SIZE;
    qmss_cfg.mem_region = Qmss_MemRegion_MEMORY_REGION0;
    res_mgr_init_qmss(&qmss_cfg);

    /* 2. 鍒濆鍖� CPPI DMA 寮曟搸 */
    cppi_cfg.master_core = 1;
    cppi_cfg.dma_num = Cppi_CpDma_PASS_CPDMA;
    cppi_cfg.num_tx_queues = NUM_PA_TX_QUEUES;
    cppi_cfg.num_rx_channels = NUM_PA_RX_CHANNELS;
    res_mgr_init_cppi(&cppi_cfg);

    /* 3. 鍒濆鍖栨暟鎹寘鍔犻�熷櫒 (PA) */
    res_mgr_init_pass();

    /* --- 浠ヤ笅涓烘爣鍑� NDK 鍚姩娴佺▼ --- */
    rc = NC_SystemOpen(NC_PRIORITY_LOW, NC_OPMODE_INTERRUPT);
    if (rc) {
        printf("[NDK] NC_SystemOpen Failed (%d)\r\n", rc);
        return;
    }

    hCfg = CfgNew();
    if (!hCfg) goto close_stack;

    if (inet_addr(LocalIPAddr)) {
        CI_IPNET NA;
        CI_ROUTE RT;

        memset(&NA, 0, sizeof(NA));
        NA.IPAddr  = inet_addr(LocalIPAddr);
        NA.IPMask  = inet_addr(LocalIPMask);
        strcpy(NA.Domain, DomainName);
        NA.NetType = 0;

        CfgAddEntry(hCfg, CFGTAG_IPNET, 2, 0, sizeof(CI_IPNET), (uint8_t *)&NA, 0);

        memset(&RT, 0, sizeof(RT));
        RT.IPDestAddr = 0;
        RT.IPDestMask = 0;
        RT.IPGateAddr = inet_addr(GatewayIP);
        CfgAddEntry(hCfg, CFGTAG_ROUTE, 0, 0, sizeof(CI_ROUTE), (uint8_t *)&RT, 0);
    }

    rc = 65536;
    CfgAddEntry(hCfg, CFGTAG_IP, CFGITEM_IP_SOCKTCPRXLIMIT, CFG_ADDMODE_UNIQUE, sizeof(uint32_t), (uint8_t *)&rc, 0);
    rc = 32768;
    CfgAddEntry(hCfg, CFGTAG_IP, CFGITEM_IP_SOCKUDPRXLIMIT, CFG_ADDMODE_UNIQUE, sizeof(uint32_t), (uint8_t *)&rc, 0);

    do {
        rc = NC_NetStart(hCfg, NetworkOpen, NetworkClose, NetworkIPAddr);
        if (rc > 0) Task_sleep(10);
    } while (rc > 0);

    CfgFree(hCfg);

close_stack:
    NC_SystemClose();
}

void ndk_task_init(void)
{
    TaskCreate(ndk_stack_task, "NDK_Stack", 8, 0x8000, 0, 0, 0);
}

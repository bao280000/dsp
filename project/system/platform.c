/**
 * Copyright (C) 2013 Guangzhou Tronlong Electronic Technology Co., Ltd. - www.tronlong.com
 *
 * @file platform.c
 *
 * @brief This file defines a set of APIs for accessing and working with
 * the various platform peripherals.
 *
 * @author Tronlong <support@tronlong.com>
 *
 * @version V1.0
 *
 * @date 2020-08-09
 *
 **/

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* CSL Header files */
#include <ti/csl/csl_chip.h>
#include <ti/csl/csl_chipAux.h>
#include <ti/csl/csl_bootcfgAux.h>
#include <ti/csl/csl_pllc.h>    /* 替代旧版 csl_pllcAux.h，新版 c667x PDK 2.x 已重组 PLLC API */

#include "platform.h"

/* JTAG ID for C6678 SOC */
#define DEVICE_C6678_JTAG_ID_VAL    0x9e02f
/* JTAG ID for C665x SOC */
#define DEVICE_C6657_JTAG_ID_VAL    0xb97a02f

/**
 * @brief This function get soc type
 *
 * @param main_pll_freq storage highest main pll frequency
 *
 * @return NULL
 */
int8_t platform_get_soc_type(void)
{
    int8_t ret = 0;
    uint32_t reg_val = 0;

    /* Get chip device id */
    CSL_BootCfgGetDeviceId(&reg_val);

    reg_val = reg_val & 0x0fffffff;
    switch(reg_val) {
    case DEVICE_C6678_JTAG_ID_VAL:
        ret = C6678;
        break;
    case DEVICE_C6657_JTAG_ID_VAL:
        ret = C665x;
        break;
    default:
        ret = -1;
        break;
    }

    return ret;
}

/**
 * @brief This function get highest chip support main pll frequency
 *
 * @param main_pll_freq storage highest main pll frequency
 *
 * @return NULL
 */
uint32_t platform_get_main_pll_freq(void)
{
    uint32_t main_pll_freq = 0;

    /* 硬件默认上电如果是 1000MHz，直接指定或从已知正确途径获得。
     * 由于 hPllc 寄存器在不完整 GEL 环境或者部分状态下读取的值可能导致
     * 浮点数计算误差严重，直接返回 1000000000 (1GHz)。 */
    main_pll_freq = 1000000000;
    
    return main_pll_freq;
}

/**
 * @brief This function creates a delay from tscl
 *
 * @param cycles cycles for delay
 *
 * @return NULL
 */
void cpu_delaycycles(uint32_t cycles)
{
    uint32_t start_val;

    /* Start TCSL so its free running */
    CSL_chipWriteTSCL(0);

    start_val = CSL_chipReadTSCL();

    while ((CSL_chipReadTSCL() - start_val) < cycles);

    return;
}

/**
 * @brief This function convert L2SARM inter address to global address
 *
 * @param addr L2SRAM inter address
 *
 * @return 32bit-addr
 */
uint32_t Convert_CoreLocal2GlobalAddr(uint32_t addr)
{
    uint32_t corenum;

    /* Get the core number. */
    corenum = CSL_chipReadReg(CSL_CHIP_DNUM);

    if((addr >= (uint32_t) 0x800000) && (addr <  (uint32_t) 0x880000)) {
        /* Compute the global address. */
        return ((1 << 28) | (corenum << 24) | (addr & 0x00ffffff));
    } else {
        return addr;
    }
}

/**
 * @brief This function get information of an EMAC port
 *
 * @param port_num port number
 *
 * @param emac_info EMAC port information
 *
 * @return 0
 */
int32_t platform_get_emac_info(uint32_t port_num, PLATFORM_EMAC_EXT_info * emac_info)
{
    uint32_t mac_addr2, mac_addr1;

    emac_info->port_num       = port_num;

    if(port_num == 0)
        emac_info->mode           = PLATFORM_EMAC_PORT_MODE_AMC;
    else if(port_num ==1)
        emac_info->mode           = PLATFORM_EMAC_PORT_MODE_PHY;

    CSL_BootCfgGetMacIdentifier(&mac_addr1, &mac_addr2);
    emac_info->mac_address[0] = ((mac_addr2 & 0x0000ff00) >> 8);
    emac_info->mac_address[1] =  (mac_addr2 & 0x000000ff);

    emac_info->mac_address[2] = ((mac_addr1 & 0xff000000) >> 24);
    emac_info->mac_address[3] = ((mac_addr1 & 0x00ff0000) >> 16);
    emac_info->mac_address[4] = ((mac_addr1 & 0x0000ff00) >> 8);
    emac_info->mac_address[5] =  (mac_addr1 & 0x000000ff);

    return 0;
}

/**
 * @brief This function get MAC address from EFUSE
 *
 * @param type MAC address storage type
 *
 * @param mac_address MAC address assigned to the core
 *
 * @return successful execution of the program
 *     @retval  0 successful
 *     @retval  Platform_EUNSUPPORTED failed
 */
int32_t platform_get_macaddr(PLATFORM_MAC_TYPE type, uint8_t *p_mac_address)
{
    uint32_t mac_addr2, mac_addr1;

    switch(type) {
    case PLATFORM_MAC_TYPE_EFUSE:
        CSL_BootCfgGetMacIdentifier(&mac_addr1, &mac_addr2);
        p_mac_address[0] = ((mac_addr2 & 0x0000ff00) >> 8);
        p_mac_address[1] =  (mac_addr2 & 0x000000ff);
        p_mac_address[2] = ((mac_addr1 & 0xff000000) >> 24);
        p_mac_address[3] = ((mac_addr1 & 0x00ff0000) >> 16);
        p_mac_address[4] = ((mac_addr1 & 0x0000ff00) >> 8);
        p_mac_address[5] =  (mac_addr1 & 0x000000ff);
        break;
    case PLATFORM_MAC_TYPE_EEPROM:
    default:
        memset(p_mac_address, 0, 6);
        return Platform_EUNSUPPORTED;
    }

    return 0;
}

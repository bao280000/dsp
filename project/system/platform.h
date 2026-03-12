/**
 * Copyright (C) 2013 Guangzhou Tronlong Electronic Technology Co., Ltd. - www.tronlong.com
 *
 * @file platform.h
 *
 * @brief This file declares a set of APIs for accessing and working with
 * the various platform peripherals.
 *
 * @author Tronlong <support@tronlong.com>
 *
 * @version V1.0
 *
 * @date 2020-08-09
 *
 **/

#ifndef PLATFORM_H_
#define PLATFORM_H_

#include <ti/platform/platform.h>

/* SOC Type */
typedef enum SOC_TYPE {
    C6678 = 1,
    C665x,
    END
}SOC_TYPE;

int8_t platform_get_soc_type(void);
uint32_t platform_get_main_pll_freq(void);
void cpu_delaycycles(uint32_t cycles);
int32_t platform_get_macaddr(PLATFORM_MAC_TYPE type, uint8_t * p_mac_address);
int32_t platform_get_emac_info(uint32_t port_num, PLATFORM_EMAC_EXT_info * emac_info);
uint32_t Convert_CoreLocal2GlobalAddr(uint32_t addr);

#endif /* #ifndef PLATFORM_H_ */

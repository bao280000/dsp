/**
 * @file uart_echo.c    (System Initialization Entry Point)
 * @brief This file has been refactored to act as a pure main.c
 *
 * @details Modularized system. Drivers are in /driver, Tasks are in /tasks.
 *
 **/

#include <stdint.h>

#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>

#include <ti/sysbios/BIOS.h>

#include <ti/csl/csl_pscAux.h>
#include <ti/csl/cslr_device.h>

#include "system/platform.h"
#include "driver/c66x_uart.h"
#include "driver/c66x_emif.h"
#include "driver/c66x_fan.h"
#include "driver/c66x_ndk.h"
#include "tasks/task_uart.h"
#include "tasks/task_ndk.h"

/**
 * @brief enable psc module
 *
 * @param void
 *
 * @return NULL
 */
void psc_init(void)
{
    /* Set psc as Always on state */
    CSL_PSC_enablePowerDomain(CSL_PSC_PD_ALWAYSON);

    /* Start state change */
    CSL_PSC_startStateTransition(CSL_PSC_PD_ALWAYSON);

    /* Wait until the status change is completed */
    while(!CSL_PSC_isStateTransitionDone(CSL_PSC_PD_ALWAYSON));

#ifdef C66_PLATFORMS
    /* Enable EMIF Clock */
    CSL_PSC_setModuleNextState(CSL_PSC_LPSC_EMIF25_SPI, PSC_MODSTATE_ENABLE);

    /* 以央PASS电源域：NDK网口需要 PKTPROC / CPGMAC / Crypto */
    CSL_PSC_enablePowerDomain(CSL_PSC_PD_PASS);
    CSL_PSC_setModuleNextState(CSL_PSC_LPSC_PKTPROC, PSC_MODSTATE_ENABLE);
    CSL_PSC_setModuleNextState(CSL_PSC_LPSC_CPGMAC,  PSC_MODSTATE_ENABLE);
    CSL_PSC_setModuleNextState(CSL_PSC_LPSC_Crypto,  PSC_MODSTATE_ENABLE);
    CSL_PSC_startStateTransition(CSL_PSC_PD_PASS);
    while(!CSL_PSC_isStateTransitionDone(CSL_PSC_PD_PASS));
#endif
}


/**
 * @brief main function
 *
 * @details Program unique entry
 *
 * @param void
 *
 * @return successful execution of the program
 *     @retval 0 successful
 *     @retval 1 failed
 */
int main(void)
{
    uint32_t main_pll_freq;

    char tips_strings[] = {"\r\nTI-DSP Modularized System Initializing...\r\n"};

    /* 1. System & Power Initialization */
    psc_init();
    
    /* 2. Hardware Peripherals Initialization */
    fan_init();
    uart_init(CSL_UART_REGS);
    emif16_init();

    /* 3. Get CPU config and set specific bus rates */
    main_pll_freq = platform_get_main_pll_freq();
    uart_set_baudrate(CSL_UART_REGS, main_pll_freq/6, 115200);

    /* 4. Display system boot message */
    uart_printf("%s", (char *)tips_strings);

    /* 5. Initialize application tasks & their specific interrupts */
    uart_task_init();

    /* 6. Initialize NDK Ethernet driver (SGMII + QMSS + CPPI + PA) */
    if(ndk_driver_init() != 0) {
        uart_printf("[NDK] ndk_driver_init failed, running without network.\r\n");
    } else {
        /* 7. Create NDK stack task (TCP Echo server) */
        ndk_task_init();
    }

    /* 6. Start BIOS real-time scheduler */
    BIOS_start();

    return 0;
}


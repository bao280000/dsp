/*
 ******************************************************************************
 * 文件名称: c66x_fan.c
 * 功能描述: TMS320C6678 板载散热风扇驱动
 *           通过 Timer8 + TOUTSEL 配置，控制 TIMO0 管脚持续输出高电平，
 *           驱动外部 NPN(Q2) + PMOS(Q1) 电路开启 12V 风扇。
 * 运行核心: Core 0 Only
 * 内存分布: L2 SRAM
 * 作者    : Zhou
 * 日期    : 2026-03-11
 ******************************************************************************
 */

#include <string.h>
#include <stdint.h>

/* CSL Header files */
#include <ti/csl/csl_tmr.h>
#include <ti/csl/csl_tmrAux.h>
#include <ti/csl/csl_bootcfgAux.h>

#include <xdc/runtime/System.h>

#include "c66x_fan.h"

/**
 * @brief 通过 Timer8 控制 TIMO0 管脚常高，启动板载散热风扇
 *
 * @details 移植自 timer_fan.c (Tronlong)。
 *          TOUTL 引脚默认低电平，配置 INVOUTP=1 将其反相，使 TIMO0 持续输出高电平。
 *          高电平经 NPN(Q2) + PMOS(Q1) 驱动电路开启风扇。
 */
void fan_init(void)
{
    CSL_TmrObj         TmrObj;
    CSL_TmrHandle      hTmr;
    CSL_Status         status;
    CSL_TmrHwSetup     hwSetup = CSL_TMR_HWSETUP_DEFAULTS;

    memset(&TmrObj, 0, sizeof(CSL_TmrObj));

    /* 解锁芯片底层寄存器 */
    CSL_BootCfgUnlockKicker();

    /* 将 TOUTSEL[4:0] 配置为 Timer8 的低边输出 TOUTL8 -> TIMO0 */
    hBootCfg->TOUTSEL &= ~(CSL_BOOTCFG_TOUTSEL_TOUTSEL0_MASK);
    hBootCfg->TOUTSEL |= ((CSL_TMR_8 * 2) << CSL_BOOTCFG_TOUTSEL_TOUTSEL0_SHIFT);

    /* 重新上锁 */
    CSL_BootCfgLockKicker();

    /* 打开 Timer8 */
    hTmr = CSL_tmrOpen(&TmrObj, CSL_TMR_8, NULL, &status);
    if(hTmr == NULL) {
        System_printf("fan_init: Open timer8 failed!\n");
        return;
    }

    /* 配置为两个独立 32-bit 定时器模式 (DUAL_UNCHAINED) */
    hwSetup.tmrTimerMode = CSL_TMR_TIMMODE_DUAL_UNCHAINED;

    /*
     * TOUTL 默认低电平。设置 INVOUTP=INVERTED 将其反相 -> 常高电平
     * 高电平 -> NPN(Q2) 导通 -> PMOS(Q1) 导通 -> 风扇得到 12V -> 风扇转动
     */
    hwSetup.tmrInvOutpLo = CSL_TMR_INVOUTP_INVERTED;

    /* 应用配置 */
    CSL_tmrHwSetup(hTmr, &hwSetup);

    /* 先复位低边定时器 */
    CSL_tmrHwControl(hTmr, CSL_TMR_CMD_RESET_TIMLO, NULL);

    /* 释放复位，使 Timer 进入可控状态（此时 INVOUTP 反相已生效，TIMO0 = 高） */
    CSL_FINST(hTmr->regs->TGCR, TMR_TGCR_TIMLORS, RESET_OFF);
}

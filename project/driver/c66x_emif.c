/* c66x_emif.c */
#include "c66x_emif.h"
#include <ti/csl/cslr_device.h>

/**
 * @brief initialize the emif16
 */
void emif16_init(void)
{
    CSL_Emif16Regs *hEmif16Cfg = ((CSL_Emif16Regs *)CSL_EMIF16_REGS);

     hEmif16Cfg->A1CR = (0 \
                         | (0 << CSL_EMIF16_A1CR_SS_SHIFT)      /* selectStrobe */ \
                         | (0 << CSL_EMIF16_A1CR_EW_SHIFT)      /* extWait */ \
                         | (1 << CSL_EMIF16_A1CR_WSETUP_SHIFT)  /* writeSetup:  1 cycle  */ \
                         | (2 << CSL_EMIF16_A1CR_WSTROBE_SHIFT) /* writeStrobe: 2 cycles */ \
                         | (1 << CSL_EMIF16_A1CR_WHOLD_SHIFT)   /* writeHold:   1 cycle  */ \
                         | (2 << CSL_EMIF16_A1CR_RSETUP_SHIFT)  /* readSetup:   2 cycles (地址到OE的建立时间加宽) */ \
                         | (6 << CSL_EMIF16_A1CR_RSTROBE_SHIFT) /* readStrobe:  6 cycles (OE保持加宽，确保FPGA数据稳定) */ \
                         | (3 << CSL_EMIF16_A1CR_RHOLD_SHIFT)   /* readHold:    3 cycles (OE收回后DSP还能稳定采样) */ \
                         | (2 << CSL_EMIF16_A1CR_TA_SHIFT)      /* turnAround:  2 cycles (两次16-bit读之间的间隔) */ \
                         | (1 << CSL_EMIF16_A1CR_ASIZE_SHIFT)); /* asyncSize:   16-bit bus */

     /* disable synchronous mode feature */
     *(volatile uint32_t *)0x20c00008 |= 0x80000000;
}

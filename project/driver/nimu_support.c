/*
 * driver/nimu_support.c
 * 为 v1 架构原生驱动提供系统级 OSAL 回调与硬件句柄绑定
 */
#include <stdint.h>
#include <xdc/std.h>
#include <ti/sysbios/family/c64p/Hwi.h>
#include <ti/sysbios/family/c64p/EventCombiner.h>
#include <ti/csl/csl_cacheAux.h>
#include <ti/csl/csl_semAux.h>
#include <ti/csl/csl_chip.h>
#include <ti/drv/qmss/qmss_drv.h>
#include <ti/drv/cppi/cppi_drv.h>
#include <ti/drv/pa/pa.h>

/* =========================================================================
 * 1. HwiP OSAL (PDK v1 驱动需要的中断抽象)
 * ========================================================================= */
uintptr_t HwiP_disable(void) { return (uintptr_t)Hwi_disable(); }
void HwiP_restore(uintptr_t key) { Hwi_restore((UInt)key); }
void HwiP_enableInterrupt(uint32_t interruptNum) { Hwi_enableInterrupt(interruptNum); }
void HwiP_disableInterrupt(uint32_t interruptNum) { Hwi_disableInterrupt(interruptNum); }

/* =========================================================================
 * 2. NIMU 硬件资源桥接 (将 resourcemgr 的句柄交给 TI 官方驱动)
 * ========================================================================= */
extern Pa_Handle     gPAInstHnd;
extern Cppi_Handle   gPassCpdmaHnd;
extern Qmss_QueueHnd gGlobalFreeQHnd;

void* NIMU_getPAInstance(void) { return (void*)gPAInstHnd; }
void* NIMU_cppiGetPASSHandle(void) { return (void*)gPassCpdmaHnd; }
Qmss_QueueHnd NIMU_qmssGetFreeQ(void) { return gGlobalFreeQHnd; }

uint32_t NIMU_convertCoreLocal2GlobalAddr(uint32_t addr) {
    uint32_t coreNum = CSL_chipReadReg(CSL_CHIP_DNUM);
    if ((addr >= 0x800000U) && (addr < 0x900000U)) {
        return ((1U << 28U) | (coreNum << 24U) | (addr & 0x00ffffffU));
    }
    return addr;
}

void NIMU_qmssQPushDescSize(Qmss_QueueHnd handler, void *descAddr, uint32_t descSize) {
    if (descAddr == NULL) return;
    CACHE_wbInvL1d(descAddr, descSize, CACHE_WAIT);
    CACHE_wbInvL2(descAddr, descSize, CACHE_WAIT);
    Qmss_queuePushDescSize(handler, descAddr, descSize);
}

/* =========================================================================
 * 3. DSP 中断与系统回调 (v1 架构特定)
 * ========================================================================= */
void NIMU_osalRegisterInterruptDsp(int16_t evt, void* fxn, void* arg, Bool unmask, int32_t vectId) {
    EventCombiner_dispatchPlug(evt, (EventCombiner_FuncPtr)fxn, (UArg)arg, unmask);
    if (unmask) EventCombiner_enableEvent(evt);
    if (evt >= 32 && evt <= 63) Hwi_enableInterrupt(8);
}

int32_t NIMU_stopCppi(uint32_t cfg_type) { return 0; }
int32_t NIMU_stopQmss(void) { return 0; }

void OEMCacheClean(void *addr, uint32_t size) {
    if (addr && size) {
        CACHE_wbL1d(addr, size, CACHE_WAIT);
        CACHE_wbL2 (addr, size, CACHE_WAIT);
    }
}

void CacheP_wbInv(void *addr, uint32_t size) {
    if (addr && size) {
        CACHE_wbInvL1d(addr, size, CACHE_WAIT);
        CACHE_wbInvL2 (addr, size, CACHE_WAIT);
    }
}

/* =========================================================================
 * 4. QMSS Accumulator 多核硬件锁
 * ========================================================================= */
#define NIMU_QMSS_ACC_HW_SEM 4

void* Osal_qmssAccCsEnter(void) {
    while ((CSL_semAcquireDirect(NIMU_QMSS_ACC_HW_SEM)) == 0);
    return NULL;
}

void Osal_qmssAccCsExit(void* CsHandle) {
    CSL_semReleaseSemaphore(NIMU_QMSS_ACC_HW_SEM);
}

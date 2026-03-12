/*
 ******************************************************************************
 * 文件名称: c66x_ndk.h
 * 功能描述: TMS320C6678 以太网底层驱动头文件
 *           包含 SGMII SERDES、QMSS、CPPI、PA 的初始化声明
 * 运行核心: Core 0 Only
 * 内存分布: L2 SRAM
 * 作者    : Zhou
 * 日期    : 2026-03-11
 ******************************************************************************
 */
#ifndef C66X_NDK_H_
#define C66X_NDK_H_

#include <stdint.h>

/**
 * @brief 初始化以太网硬件子系统
 *
 * @details 执行顺序：
 *          1. PSC 使能 PASS/CPGMAC/Crypto 电源域
 *          2. SGMII SERDES 配置（Port 0 & Port 1，自动协商 1G 全双工）
 *          3. QMSS 队列管理子系统初始化
 *          4. CPPI 包传输子系统初始化
 *          5. PA  包加速器初始化
 *          必须在调用 BIOS_start() 之前完成，且仅由 Core 0 执行。
 *
 * @return  0 = 成功，-1 = 失败
 */
int32_t ndk_driver_init(void);

#endif /* C66X_NDK_H_ */

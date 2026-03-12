/*
 ******************************************************************************
 * 文件名称: task_emif_test.h
 * 功能描述: FPGA EMIF RAM 读写测试任务头文件
 *           测试 FPGA 端 RAM 地址范围 0x1000~0x2000 的读写正确性
 * 运行核心: Core 0 Only
 * 内存分布: L2 SRAM
 * 作者    : Zhou
 * 日期    : 2026-03-11
 ******************************************************************************
 */
#ifndef TASK_EMIF_TEST_H_
#define TASK_EMIF_TEST_H_

/**
 * @brief 启动 EMIF RAM 读写测试
 *
 * @details 对 FPGA 端 0x1000~0x2000 地址区间进行多模式读写验证，
 *          通过 UART 实时输出进度和最终测试报告。
 */
void emif_ram_test(void);

#endif /* TASK_EMIF_TEST_H_ */

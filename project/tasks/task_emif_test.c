/*
 ******************************************************************************
 * 文件名称: task_emif_test.c
 * 功能描述: FPGA EMIF RAM 读写测试任务
 *           对 FPGA 端 0x1000~0x2000 地址空间进行多模式读写验证：
 *           1. 固定图案 (0xAA55AA55 / 0x5555AAAA)
 *           2. 地址递增图案
 *           3. 行走 1 测试 (Walking-1)
 * 运行核心: Core 0 Only
 * 内存分布: L2 SRAM
 * 作者    : Zhou
 * 日期    : 2026-03-11
 ******************************************************************************
 */

#include <stdint.h>
#include <string.h>

#include <xdc/runtime/System.h>

#include "task_emif_test.h"
#include "../driver/c66x_emif.h"
#include "../driver/c66x_uart.h"

/* EMIF RAM 测试地址范围 (用户输入的 FPGA 地址偏移) */
#define EMIF_TEST_START  0x1000u              /* 起始地址 (4字节对齐) */
#define EMIF_TEST_END    0x2000u              /* 结束地址 (不含) */
#define EMIF_TEST_STEP   4u                   /* 32-bit 字步进 */
#define EMIF_TEST_COUNT  ((EMIF_TEST_END - EMIF_TEST_START) / EMIF_TEST_STEP)

/* -----------------------------------------------------------------------
 * 内部辅助函数
 * ---------------------------------------------------------------------- */

/**
 * @brief 向 FPGA 指定偏移地址写入 32-bit 数据
 */
static inline void emif_write(uint32_t offset, uint32_t data)
{
    volatile uint32_t *p = (volatile uint32_t *)(CS3_MEMORY_DATA_ADDR + (offset & 0x00FFFFFF));
    *p = data;
}

/**
 * @brief 从 FPGA 指定偏移地址读取 32-bit 数据
 */
static inline uint32_t emif_read(uint32_t offset)
{
    volatile uint32_t *p = (volatile uint32_t *)(CS3_MEMORY_DATA_ADDR + (offset & 0x00FFFFFF));
    return *p;
}

/**
 * @brief 单次图案写入-校验子测试
 *
 * @param pattern_name   测试名称（用于打印）
 * @param pattern_func   指向图案生成函数，入参为 32-bit 地址偏移
 *
 * @return 0 = PASS，>0 = 失败次数
 */
typedef uint32_t (*pattern_fn)(uint32_t offset);

static uint32_t run_pattern_test(const char *pattern_name, pattern_fn gen)
{
    uint32_t addr, expected, actual;
    uint32_t fail_cnt = 0;
    uint32_t first_fail_addr = 0;
    uint32_t first_fail_exp  = 0;
    uint32_t first_fail_got  = 0;

    uart_printf("[TEST] %-20s  Write...", pattern_name);

    /* 写入阶段 */
    for (addr = EMIF_TEST_START; addr < EMIF_TEST_END; addr += EMIF_TEST_STEP) {
        emif_write(addr, gen(addr));
    }

    uart_printf(" Read...");

    /* 校验阶段 */
    for (addr = EMIF_TEST_START; addr < EMIF_TEST_END; addr += EMIF_TEST_STEP) {
        expected = gen(addr);
        actual   = emif_read(addr);
        if (actual != expected) {
            if (fail_cnt == 0) {
                first_fail_addr = addr;
                first_fail_exp  = expected;
                first_fail_got  = actual;
            }
            fail_cnt++;
        }
    }

    if (fail_cnt == 0) {
        uart_printf(" \r[TEST] %-20s  PASS  (%u words verified)\r\n",
                    pattern_name, EMIF_TEST_COUNT);
    } else {
        uart_printf(" \r[TEST] %-20s  FAIL  (%u errors, first@0x%04X exp=0x%08X got=0x%08X)\r\n",
                    pattern_name, fail_cnt,
                    first_fail_addr, first_fail_exp, first_fail_got);
    }

    return fail_cnt;
}

/* -----------------------------------------------------------------------
 * 图案生成函数
 * ---------------------------------------------------------------------- */

static uint32_t pat_aa55(uint32_t offset)    { (void)offset; return 0xAA55AA55u; }
static uint32_t pat_5555(uint32_t offset)    { (void)offset; return 0x5555AAAAu; }
static uint32_t pat_addr(uint32_t offset)    { return offset; }
static uint32_t pat_inv_addr(uint32_t offset){ return ~offset; }

/* -----------------------------------------------------------------------
 * Walking-1 测试 (每次翻转 1 个 bit)
 * ---------------------------------------------------------------------- */
static uint32_t walking1_test(void)
{
    uint32_t bit, addr, actual, expected;
    uint32_t fail_cnt = 0;

    uart_printf("[TEST] %-20s  ", "Walking-1");

    for (bit = 0; bit < 32; bit++) {
        uint32_t val = (1u << bit);

        /* 写整块 */
        for (addr = EMIF_TEST_START; addr < EMIF_TEST_END; addr += EMIF_TEST_STEP)
            emif_write(addr, val);

        /* 校验 */
        for (addr = EMIF_TEST_START; addr < EMIF_TEST_END; addr += EMIF_TEST_STEP) {
            expected = val;
            actual   = emif_read(addr);
            if (actual != expected) {
                if (fail_cnt == 0)
                    uart_printf("\r\n  [FAIL] bit%02u addr=0x%04X exp=0x%08X got=0x%08X",
                                bit, addr, expected, actual);
                fail_cnt++;
            }
        }
    }

    if (fail_cnt == 0)
        uart_printf("PASS  (32 bits x %u words)\r\n", EMIF_TEST_COUNT);
    else
        uart_printf("\r\n  FAIL  (%u errors total)\r\n", fail_cnt);

    return fail_cnt;
}

/* -----------------------------------------------------------------------
 * 公开接口
 * ---------------------------------------------------------------------- */

/**
 * @brief 启动 EMIF RAM 读写测试
 */
void emif_ram_test(void)
{
    uint32_t total_fail = 0;

    uart_printf("\r\n========================================\r\n");
    uart_printf("  EMIF RAM Test  [0x%04X - 0x%04X]\r\n", EMIF_TEST_START, EMIF_TEST_END);
    uart_printf("  共 %u 个 32-bit 字 (%u 字节)\r\n",
                EMIF_TEST_COUNT, EMIF_TEST_COUNT * 4);
    uart_printf("========================================\r\n");

    /* 测试 1：固定图案 0xAA55AA55 */
    total_fail += run_pattern_test("Pattern 0xAA55AA55", pat_aa55);

    /* 测试 2：固定图案 0x5555AAAA */
    total_fail += run_pattern_test("Pattern 0x5555AAAA", pat_5555);

    /* 测试 3：地址递增图案 */
    total_fail += run_pattern_test("Addr-Increment    ", pat_addr);

    /* 测试 4：地址取反图案 */
    total_fail += run_pattern_test("Addr-Inverted     ", pat_inv_addr);

    /* 测试 5：Walking-1 */
    total_fail += walking1_test();

    /* 汇总 */
    uart_printf("========================================\r\n");
    if (total_fail == 0)
        uart_printf("  All Tests  >>  PASS  <<\r\n");
    else
        uart_printf("  All Tests  >>  FAIL  << (%u total errors)\r\n", total_fail);
    uart_printf("========================================\r\n\r\n");
}

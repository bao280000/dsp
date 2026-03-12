#include "task_uart.h"
#include <stdio.h>
#include <string.h>

#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>

#include <ti/sysbios/knl/Semaphore.h>
#include <ti/csl/cslr_device.h>
#include <ti/csl/cslr_uart.h>
#include <ti/sysbios/family/c64p/Hwi.h>
#include <ti/sysbios/family/c66/tci66xx/CpIntc.h>

#include "../driver/c66x_uart.h"
#include "../driver/c66x_emif.h"
#include "task_emif_test.h"

/* 行输入缓冲区大小 */
#define INPUT_BUF_SIZE  128

/* 环形缓冲区配置 */
#define RX_BUF_SIZE 256
static uint8_t rx_buf[RX_BUF_SIZE];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;
static Semaphore_Handle rx_sem = NULL;

/* 工业级诊断：记录软硬件溢出或线路错误次数 */
volatile uint32_t uart_error_cnt = 0;
volatile uint32_t rx_overflow_cnt = 0;

/**
 * @brief UART 接收中断服务函数 (ISR)
 *
 * @details 硬件触发，处理接收数据及线路错误异常，防死锁
 */
void uart_rx_isr(UArg arg)
{
    CSL_UartRegs *regs = (CSL_UartRegs *)CSL_UART_REGS;

    /* 读取 ISR 状态：IIR 的 Bit0 为 0 表示有中断挂起 */
    while ((regs->IIR & 0x01) == 0) {
        /* 读取中断原因，用直接位运算代替可能因版而异的 CSL_FEXT 掩码常数。
         * Bit1~3 决定中断源。连同 Bit0 一起看的话：
         * 0x06 (0110b) -> Receiver Line Status (线路异常如溢出/帧错)
         * 0x04 (0100b) -> Rx Data Available
         * 0x0C (1100b) -> Character Timeout */
        uint8_t cause = regs->IIR & 0x0F;
        
        /* 1. 致命异常恢复：硬件 FIFO 溢出 (OE)、奇偶校验错 (PE)、帧错 (FE) 或 Break */
        if (cause == 0x06) {
            /* 必须读取 LSR(Line Status Register) 以清除这几种错误中断挂起标志，
             * 否则硬件会永远霸占中断线导致整个系统死锁！ */
            volatile uint8_t lsr_dummy = regs->LSR; 
            uart_error_cnt++;
            (void)lsr_dummy; /* 抑制未使用的警告 */
        }
        /* 2. 正常数据接收和超时等待接收 */
        else if (cause == 0x04 || cause == 0x0C) { 
            /* 只要 Rx FIFO 还有数据 (LSR Bit0/DR 位为1) */
            while ((regs->LSR & 0x01) != 0) {
                uint8_t ch = regs->RBR;  /* 读 RBR 释放硬件 FIFO 空间 */
                uint16_t next = (rx_head + 1) & (RX_BUF_SIZE - 1);
                
                /* 如果软件环形缓冲未满，存入数据 */
                if (next != rx_tail) {
                    rx_buf[rx_head] = ch;
                    rx_head = next;
                } else {
                    /* 处理速度跟不上！记录丢包，新来的数据直接丢弃 */
                    rx_overflow_cnt++;
                }
            }
            /* 通知处理任务，唤醒后续动作 */
            if (rx_sem != NULL) {
                Semaphore_post(rx_sem);
            }
        }
    }
}

void uart_echo(UArg arg0, UArg arg1)
{
    uint8_t  input_buf[INPUT_BUF_SIZE];
    uint16_t input_len = 0;

    /* 初始化信号量 (初始值0) */
    rx_sem = Semaphore_create(0, NULL, NULL);

    while(1) {
        /* 显示输入提示符 */
        uart_printf("\r\n> 请输入内容（Enter 提交）：");
        input_len = 0;

        /* 逐字符接收，直到回车键 */
        while(1) {
            /* 阻塞等待硬件中断唤醒，避免完全占用 CPU，提高系统效率 */
            Semaphore_pend(rx_sem, BIOS_WAIT_FOREVER);
            
            /* 处理刚刚收入环形缓冲的所有数据 */
            while (rx_tail != rx_head) {
                uint8_t ch = rx_buf[rx_tail];
                rx_tail = (rx_tail + 1) & (RX_BUF_SIZE - 1);

                if(ch == '\r' || ch == '\n') {
                    if (input_len == 0) {
                        /* 忽略连续的换行/空回车（例如 \r 紧接着的 \n） */
                        continue;
                    }
                    /* 收到回车，结束本次输入，解析命令或回传 */
                    input_buf[input_len] = '\0';
                    
                    uint32_t addr = 0;
                    uint32_t data = 0;
                    
                    /* EMIF RAM 自动测试 */
                    if (strncmp((char *)input_buf, "emiftest", 8) == 0) {
                        emif_ram_test();
                    }
                    /* 解析 EMIF 写操作：wr+addr(24bit hex)+data(32bit hex) */
                    else if (strncmp((char *)input_buf, "wr+", 3) == 0) {
                        if (sscanf((char *)input_buf, "wr+%x+%x", &addr, &data) == 2) {
                            /* 因为地址是24bit所以屏蔽高位，叠加上CS3基地址。
                             * SRAM/FPGA侧数据位宽为32bit（通过2次16bit存取完成）。
                               由于DSP外部地址的单位是byte，用户输入的addr可能是针对32bit或只是一般offset，
                               保持 offset = addr */
                            volatile uint32_t *emif_ptr = (volatile uint32_t *)(CS3_MEMORY_DATA_ADDR + (addr & 0x00FFFFFF));
                            *emif_ptr = data;
                            uart_printf("\r\n[EMIF Write] Addr: 0x%08X = 0x%08X OK.\r\n", (uint32_t)emif_ptr, data);
                        } else {
                            uart_printf("\r\n[命令错误] 格式应为 wr+<addr>+<data> (如 wr+000100+12345678)\r\n");
                        }
                    } 
                    /* 解析 EMIF 读操作：rd+addr(24bit hex) */
                    else if (strncmp((char *)input_buf, "rd+", 3) == 0) {
                        if (sscanf((char *)input_buf, "rd+%x", &addr) == 1) {
                            volatile uint32_t *emif_ptr = (volatile uint32_t *)(CS3_MEMORY_DATA_ADDR + (addr & 0x00FFFFFF));
                            data = *emif_ptr;
                            uart_printf("\r\n[EMIF Read]  Addr: 0x%08X = 0x%08X OK.\r\n", (uint32_t)emif_ptr, data);
                        } else {
                            uart_printf("\r\n[命令错误] 格式应为 rd+<addr> (如 rd+000100)\r\n");
                        }
                    }
                    /* 普通文本回显 */
                    else {
                        uart_printf("\r\n[ECHO] 收到 %d 字节 >>> %s\r\n",
                                    (int)input_len, (char *)input_buf);
                    }
                    
                    /* 设置标记突破上层循环重新打印提示符 */
                    input_len = 0xFFFF;
                    break;
                } else if((ch == 0x08) || (ch == 0x7F)) {
                    /* Backspace / Delete：删除上一字符 */
                    if(input_len > 0) {
                        input_len--;
                        /* 光标后退、空格覆盖、再次后退，实现终端删除效果 */
                        uart_printf("\b \b");
                    }
                } else if(input_len < (INPUT_BUF_SIZE - 1)) {
                    /* 普通可打印字符：存入缓冲并本地回显 */
                    input_buf[input_len++] = ch;
                    uart_printf("%c", ch);
                }
            }
            
            if(input_len == 0xFFFF) {
                break;
            }
        }
    }
}

/**
 * @brief 初始化 UART 相关的系统中断，并创建回显任务
 */
void uart_task_init(void)
{
    Task_Handle task;
    Error_Block eb;
    Task_Params TaskParams;

    Error_init(&eb);

    /* ================ CPINTC (Interrupt) Routing ================ */
    /* C6678 UART0 System Event is 148 (CSL_INTC0_UARTINT), >127, must route thru CPINTC.
     * 1. Map System Event 148 to Host Interrupt 42 */
    CpIntc_mapSysIntToHostInt(0, 148, 42);

    /* 2. Plug the ISR for System Event 148 */
    CpIntc_dispatchPlug(148, (CpIntc_FuncPtr)uart_rx_isr, 148, TRUE);

    /* 3. Enable System Event 148 */
    CpIntc_enableSysInt(0, 148);

    /* 4. Enable Host Interrupt 42 */
    CpIntc_enableHostInt(0, 42);

    /* 5. Enable Global Host Interrupts across the chip */
    CpIntc_enableAllHostInts(0);

    /* ================ DSP Core INTC Mapping  ================ */
    {
        Hwi_Params hwiParams;
        Hwi_Params_init(&hwiParams);
        /* 将 CpIntc 的 HostInt 42 翻译为 DSP 可见的核心事件 ID (如 21) */
        hwiParams.eventId = CpIntc_getEventId(42);
        hwiParams.arg = 42;
        hwiParams.enableInt = TRUE;
        
        /* 先清空可能因为上电电平抖动导致的历史事件挂起 */
        CpIntc_clearSysInt(0, 148);

        /* 在 CPU 中断向量 4 上创建 Hwi，指向 CpIntc 的全局分发器 (将会最终派发给 uart_rx_isr) */
        Hwi_create(4, &CpIntc_dispatch, &hwiParams, &eb);
    }

    /* create a Task, which is uart_echo */
    Task_Params_init(&TaskParams);
    task = Task_create(uart_echo, &TaskParams, &eb);
    if(task == NULL) {
        System_printf("Task_create() failed!\n");
        BIOS_exit(0);
    }
}

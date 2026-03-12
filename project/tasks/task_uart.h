#ifndef _TASK_UART_H_
#define _TASK_UART_H_

#include <stdint.h>
#include <xdc/std.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

/* 中断与任务初始化 */
void uart_task_init(void);

#endif /* _TASK_UART_H_ */

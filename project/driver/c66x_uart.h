/**
 * Copyright (C) 2013 Guangzhou Tronlong Electronic Technology Co., Ltd. - www.tronlong.com
 *
 * @file c66x_uart.h
 *
 * @brief This file is the header file for UART module.
 *
 * @author Tronlong <support@tronlong.com>
 *
 * @version V1.0
 *
 * @date 2020-08-09
 *
 **/

#ifndef _C66X_UART_H_
#define _C66X_UART_H_

#include <xdc/std.h>
#include <stdint.h>

/* ================ Macros ================ */
/** default baudrate(19200) lower divisor */
#define DLL_VAL     (0x30)
/** default baudrate(19200) higher divisor */
#define DLH_VAL     (0x00)

/** max string length for uart_printf */
#define MAX_WRITE_LEN (256)

/* ================ Function declarations ================ */
void uart_init(uint32_t uartreg);
void uart_set_baudrate(uint32_t uartreg, uint32_t uart_iclk, uint32_t baudrate);
uint16_t uart_read_baudrate(uint32_t uartreg);
uint8_t uart_read_data(uint32_t uartreg);
void uart_write_data(uint32_t uartreg, uint8_t byte);
void uart_printf(char *fmt, ...);
Bool uart_isdata_ready(uint32_t uartreg);

#endif /* #ifndef _C66X_UART_H_ */

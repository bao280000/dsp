/**
 * Copyright (C) 2013 Guangzhou Tronlong Electronic Technology Co., Ltd. - www.tronlong.com
 *
 * @file c66x_uart.c
 *
 * @brief This file contains the lower level function to access UART device.
 *
 * @author Tronlong <support@tronlong.com>
 *
 * @version V1.0
 *
 * @date 2020-08-09
 *
 **/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/* CSL Header file */
#include <ti/csl/cslr_uart.h>
#include <ti/csl/cslr_device.h>
#include <ti/csl/csl_types.h>

#include "c66x_uart.h"

/**
 * @brief This function initializes the UART system
 *
 * @param base_address uart register base address
 *
 * @return NULL
 */
void uart_init(uint32_t base_address)
{
    CSL_UartRegs *uart_reg = (CSL_UartRegs *)base_address;

    /*
     * Allows access to the divisor latches of the baud generator during a
     * read or write operation (DLL and DLH)
     */
    CSL_FINS(uart_reg->LCR, \
            UART_LCR_DLAB, CSL_UART_LCR_DLAB_ENABLE);
    /* Break condition is disabled */
    CSL_FINS(uart_reg->LCR, \
            UART_LCR_BC, CSL_UART_LCR_BC_DISABLE);
    /* Stick parity is disabled */
    CSL_FINS(uart_reg->LCR, \
            UART_LCR_SP, CSL_UART_LCR_SP_DISABLE);
    /* Odd parity is selected */
    CSL_FINS(uart_reg->LCR, \
            UART_LCR_EPS, CSL_UART_LCR_EPS_ODD);
    /* No PARITY bit is transmitted or checked */
    CSL_FINS(uart_reg->LCR, \
            UART_LCR_PEN, CSL_UART_LCR_PEN_DISABLE);

    /* Set the baudrate,for accessing LCR[7] should be enable */
    uart_reg->DLL = DLL_VAL;
    uart_reg->DLH = DLH_VAL;

    /*
     * Allows access to the receiver buffer register (RBR)
     * the transmitter holding register (THR), and the
     * interrupt enable register (IER) selected.
     */
    CSL_FINS(uart_reg->LCR, \
            UART_LCR_DLAB, CSL_UART_LCR_DLAB_DISABLE);
    /* Even Parity is selected */
    CSL_FINS(uart_reg->LCR, \
            UART_LCR_EPS, CSL_UART_LCR_EPS_EVEN);
    /* Parity Enable */
    CSL_FINS(uart_reg->LCR, \
            UART_LCR_PEN, CSL_UART_LCR_PEN_ENABLE);

    /* Enable Receiver line status interrupts (ERBI) */
    CSL_FINS(uart_reg->IER, \
            UART_IER_ERBI, CSL_UART_IER_ERBI_ENABLE);
    CSL_FINS(uart_reg->IER, \
            UART_IER_ETBEI, CSL_UART_IER_ETBEI_DISABLE);
    CSL_FINS(uart_reg->IER, \
            UART_IER_ELSI, CSL_UART_IER_ELSI_DISABLE);
    CSL_FINS(uart_reg->IER, \
            UART_IER_EDSSI, CSL_UART_IER_EDSSI_DISABLE);

    /*
     * If autoflow control is desired,
     * write appropriate values to the modem
     * control register (MCR). Note that all UARTs
     * do not support autoflow control, see
     * the device-specific data manual for supported features.
     *
     * MCR
     * ====================================================
     * Bit  Field   Value   Description
     * 5    AFE     0       Autoflow control is disabled
     * 4    LOOP    0       Loop back mode is disabled.
     * 1    RTS     0       RTS control (UARTn_RTS is disabled,
     *                      UARTn_CTS is only enabled.)
     * =====================================================
     */
    uart_reg->MCR = 0;

    /*
     * Choose the desired response to
     * emulation suspend events by configuring
     * the FREE bit and enable the UART by setting
     * the UTRST and URRST bits in the power and
     * emulation management register (PWREMU_MGMT).
     *
     * PWREMU_MGMT
     * =================================================
     * Bit  Field   Value   Description
     * 14   UTRST   1       Transmitter is enabled
     * 13   URRST   1       Receiver is enabled
     * 0    FREE    1       Free-running mode is enabled
     * ===================================================
     */
    uart_reg->PWREMU_MGMT = 0x6001;

    /* Cleanup previous data (rx trigger is also set to 0)*/
    /* Set FCR = 0x07 */
    CSL_FINS(uart_reg->FCR, UART_FCR_FIFOEN, CSL_UART_FCR_FIFOEN_ENABLE);
    CSL_FINS(uart_reg->FCR, UART_FCR_TXCLR, CSL_UART_FCR_TXCLR_CLR);
    CSL_FINS(uart_reg->FCR, UART_FCR_RXCLR, CSL_UART_FCR_RXCLR_CLR);
    CSL_FINS(uart_reg->FCR, UART_FCR_DMAMODE1, CSL_UART_FCR_DMAMODE1_DISABLE);
    CSL_FINS(uart_reg->FCR, UART_FCR_RXFIFTL, CSL_UART_FCR_RXFIFTL_CHAR1);
}

/**
 * @brief This function sets the UART baudrate
 *
 * @param base_address uart register base address
 *
 * @param uart_iclk uart input clock
 *
 * @param baudrate uart baudrate to be set
 *
 * @return NULL
 */
void uart_set_baudrate(uint32_t base_address, uint32_t uart_iclk, uint32_t baudrate)
{
    uint16_t divisor = 0;
    uint8_t dll_val = 0;
    uint8_t dlh_val = 0;

    CSL_UartRegs *uart_reg = (CSL_UartRegs *)base_address;

    /* divisor = uart input clock / (baudrate * 16) */
    divisor = ((Uint16)(uart_iclk / (baudrate * 16)));

    /* dll = divisor[0:7], dlh = divisor[7:15] */
    dll_val = (uint8_t )(CSL_UART_DLL_DLL_MASK & divisor);
    dlh_val = (uint8_t )(CSL_UART_DLH_DLH_MASK & (divisor  >> 8));

    /* Set the baudrate,for accessing LCR[7] should be enable */
    uart_reg->LCR = (1 << CSL_UART_LCR_DLAB_SHIFT);
    uart_reg->DLL = dll_val;
    uart_reg->DLH = dlh_val;

    /* Set word length as 8 bits */
    uart_reg->LCR = (0x3 << CSL_UART_LCR_WLS_SHIFT);
}

/**
 * @brief This function reads the UART baudrate
 *
 * @param base_address uart register base address
 *
 * @return 16-bits baudrate
 */
uint16_t uart_read_baudrate(uint32_t base_address)
{
    uint16_t divisor_low = 0;
    uint16_t divisor_high = 0;
    uint16_t baudrate = 0;

    CSL_UartRegs *uart_reg = (CSL_UartRegs *)base_address;

    /* Read the baudrate,for accessing LCR[7] should be enable */
    uart_reg->LCR = (1 << CSL_UART_LCR_DLAB_SHIFT);

    divisor_low = uart_reg->DLL;
    divisor_high = uart_reg->DLH;
    baudrate = (divisor_low & CSL_UART_DLL_DLL_MASK) | \
                ((divisor_high & CSL_UART_DLH_DLH_MASK) << 8);

    /* Set word length as 8 bits */
    uart_reg->LCR = (0x3 << CSL_UART_LCR_WLS_SHIFT);

    return baudrate;
}

/**
 * @brief This function reads a byte of data from UART device
 *
 * @param base_address uart register base address
 *
 * @return 8-bits value, which is read from the RBR register
 */
uint8_t uart_read_data(uint32_t base_address)
{
    uint8_t rcv = 0;

    CSL_UartRegs *uart_reg = (CSL_UartRegs *)base_address;

    rcv = CSL_FEXT(uart_reg->RBR, UART_RBR_DATA);

    return rcv;
}

/**
 * @brief This function echo 8-bits data to UART device
 *
 * @param base_address uart register base address
 *
 * @param data data for sending
 *
 * @return NULL
 */
void uart_write_data(uint32_t base_address, uint8_t data)
{
    CSL_UartRegs *uart_reg = (CSL_UartRegs *)base_address;

    while (!(CSL_FEXT(uart_reg->LSR, UART_LSR_THRE)));

    /* 注意：THR 是只写寄存器，必须直接赋值写入！
     * 若使用 CSL_FINS（先读后写），读 THR 地址实际读的是 RBR——
     * 会把接收 FIFO 中的下一个字节偷偷消耗掉，导致接收数据丢失。 */
    uart_reg->THR = data;
}

/**
 * @brief This function echo string data to UART device
 *
 * @param fmt string data for sending
 *
 * @return NULL
 *
 * @note default only supports serial port 0
 */
void uart_printf(char *fmt, ...)
{
    va_list     arg_ptr;
    uint32_t    i, length;
    static char write_buffer[MAX_WRITE_LEN];

    va_start(arg_ptr, fmt);
    length = vsprintf((char *)write_buffer, fmt, arg_ptr);
    va_end(arg_ptr);

    for (i = 0; i < length; i++) {
        if (write_buffer[i] == '\n') {
            (void)uart_write_data(CSL_UART_REGS, (uint8_t)0x0D);
            (void)uart_write_data(CSL_UART_REGS, (uint8_t)0x0A);
        } else {
            (void)uart_write_data(CSL_UART_REGS, (uint8_t)write_buffer[i]);
        }
    }
}

/**
 * @brief This function gets the uart receive status
 *
 * @param base_address uart register base address
 *
 * @return receive status
 *     @retval TRUE data is ready
 *     @retval FALSE data is not ready
 */
Bool uart_isdata_ready(uint32_t base_address)
{
    Bool status = FALSE;

    CSL_UartRegs *uart_reg = (CSL_UartRegs *)base_address;

    if (CSL_UART_LSR_DR_READY == (CSL_FEXT(uart_reg->LSR, UART_LSR_DR)))
        status  = TRUE;

    return status;
}


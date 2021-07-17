#ifndef _UART_DRIVER_H_
#define _UART_DRIVER_H_

#include <stdint.h>
#include <stdlib.h>

#define fifo_TX_BUF_SIZE			64
#define fifo_RX_BUF_SIZE			64
#define EOF							-1


void configure_uart(int baudrate);
void write_flush();
void write_char(const char data);
/*int get_rx_getc();
char get_rx_getc_blocking();
*/

size_t rx_get_available();
size_t read(char* target, size_t count);
void write(const char* data, size_t length);

#endif
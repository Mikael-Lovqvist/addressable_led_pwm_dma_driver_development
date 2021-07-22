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
void read_blocking(char* target, size_t required_count);
bool rx_had_overrun();


typedef void(*uart_event_callback_type)(void);
void configure_uart2(int baudrate, uint32_t target, uint32_t target_size, uart_event_callback_type transfer_complete_cb);	//Temporary test method

#endif
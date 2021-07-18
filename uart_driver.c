/*
	This uart driver is a subset of the more advanced driver being developed at /home/devilholk/Projects/stm32_unit_testing
	and also at /home/devilholk/Projects/c_gen_for_stm .

*/


#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/cm3/nvic.h>
#include "dma_utils.h"
#include "uart_driver.h"

static volatile struct {
	int head;
	int tail;
	bool transfer_pending;
	char buf[fifo_TX_BUF_SIZE];
} tx_buf;

static volatile struct {
	//head is in the DMA peripheral
	int tail;
	int balance;	//Used to track overrun
	bool had_overrun;
	char buf[fifo_RX_BUF_SIZE];
} rx_buf;




void configure_uart(int baudrate) {

	rcc_periph_clock_enable(RCC_DMA1);
	rcc_periph_clock_enable(RCC_USART1);


	usart_set_baudrate(USART1, baudrate);
	usart_set_databits(USART1, 8);
	usart_set_stopbits(USART1, USART_STOPBITS_1);
	usart_set_mode(USART1, USART_MODE_RX | USART_MODE_TX);
	usart_set_parity(USART1, USART_PARITY_NONE);
	usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);
	usart_enable(USART1);


	//Set up transmission DMA
	dma_channel_reset(DMA1, DMA_CHANNEL4);
	dma_set_peripheral_address(DMA1, DMA_CHANNEL4, (uint32_t) &USART1_DR);
	dma_set_read_from_memory(DMA1, DMA_CHANNEL4);
	dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL4);
	dma_set_peripheral_size(DMA1, DMA_CHANNEL4, DMA_CCR_PSIZE_8BIT);
	dma_set_memory_size(DMA1, DMA_CHANNEL4, DMA_CCR_MSIZE_8BIT);
	dma_set_priority(DMA1, DMA_CHANNEL4, DMA_CCR_PL_VERY_HIGH);

	//Set up reception dMA
	dma_channel_reset(DMA1, DMA_CHANNEL5);
	dma_set_peripheral_address(DMA1, DMA_CHANNEL5, (uint32_t) &USART1_DR);
	dma_set_read_from_peripheral(DMA1, DMA_CHANNEL5);
	dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL5);
	dma_set_peripheral_size(DMA1, DMA_CHANNEL5, DMA_CCR_PSIZE_8BIT);
	dma_set_memory_size(DMA1, DMA_CHANNEL5, DMA_CCR_MSIZE_8BIT);
	dma_enable_circular_mode(DMA1, DMA_CHANNEL5);
	dma_set_priority(DMA1, DMA_CHANNEL5, DMA_CCR_PL_VERY_HIGH);

	dma_set_memory_address(DMA1, DMA_CHANNEL5, (uint32_t) rx_buf.buf);
	dma_set_number_of_data(DMA1, DMA_CHANNEL5, fifo_RX_BUF_SIZE);

	dma_enable_half_transfer_interrupt(DMA1, DMA_CHANNEL5);
	dma_enable_transfer_complete_interrupt(DMA1, DMA_CHANNEL5);

	dma_enable_channel(DMA1, DMA_CHANNEL5);


	nvic_enable_irq(NVIC_DMA1_CHANNEL5_IRQ);
	usart_enable_rx_dma(USART1);

}


static void write_flush_once() {

	if (!tx_buf.transfer_pending) {
		return;
	}


	if (DMA_CCR(DMA1, DMA_CHANNEL4) & DMA_CCR_EN) {
		//Block and clear if a transfer is pending (every time except the first time)
		while (!dma_get_interrupt_flag(DMA1, DMA_CHANNEL4, DMA_TCIF));
		dma_clear_interrupt_flags(DMA1, DMA_CHANNEL4, DMA_TCIF);
	}

	int to_flush = tx_buf.tail - tx_buf.head;
	if (to_flush < 0) {
		//Only flush to end this time around
		//We also don't clear transfer_pending since we need another flush
		to_flush = fifo_TX_BUF_SIZE - tx_buf.head;

	} else if (to_flush == 0) {
		return;
	} else {
		tx_buf.transfer_pending = false;
	}



	//Flush data
	usart_disable_tx_dma(USART1);
	dma_disable_channel(DMA1, DMA_CHANNEL4);
	dma_set_memory_address(DMA1, DMA_CHANNEL4, (uint32_t) tx_buf.buf + tx_buf.head);
	dma_set_number_of_data(DMA1, DMA_CHANNEL4, to_flush);
	dma_enable_channel(DMA1, DMA_CHANNEL4);
	usart_enable_tx_dma(USART1);

	//Advance head by flushed amount positions
	tx_buf.head = (tx_buf.head + to_flush) % fifo_TX_BUF_SIZE;

}

void write_flush() {
	while(tx_buf.transfer_pending) {
		write_flush_once();
	}
}

void write_char(const char data) {
	typeof(tx_buf.tail) pending_tail = (tx_buf.tail + 1) % fifo_TX_BUF_SIZE;
	if (pending_tail == tx_buf.head) {
		tx_buf.transfer_pending = true;
		write_flush();
	}
	tx_buf.transfer_pending = true;
	tx_buf.buf[tx_buf.tail] = data;
	tx_buf.tail = pending_tail;
}


void write(const char* data, size_t length) {
	for (int i=0; i<length; i++) {
		write_char(data[i]);
	}
}




size_t rx_get_available() {

	int head = fifo_RX_BUF_SIZE - dma_get_number_of_data(DMA1, DMA_CHANNEL5);

	int available = head - rx_buf.tail;

	if (available < 0) {
		return available + fifo_RX_BUF_SIZE;
	} else {
		return available;
	}

}


size_t read(char* target, size_t count) {	


	int available = rx_get_available();

	if (count > available) {
		count = available;
	}

	if (count == 0) {
		return 0;
	}


	for (int i=0; i<count; i++) {
		target[i] = rx_buf.buf[rx_buf.tail];
		rx_buf.tail = (rx_buf.tail + 1) % fifo_RX_BUF_SIZE;
	}

	nvic_disable_irq(NVIC_DMA1_CHANNEL5_IRQ);
	rx_buf.balance -= count;
	nvic_enable_irq(NVIC_DMA1_CHANNEL5_IRQ);
	return count;
}




void dma1_channel5_isr(void) {
	uint32_t isr_flags = dma_get_interrupt_flags(DMA1, DMA_CHANNEL5) & (DMA_TCIF | DMA_HTIF);

	if (isr_flags) {

		if (rx_buf.balance < (fifo_RX_BUF_SIZE >> 1)) {
			rx_buf.balance += (fifo_RX_BUF_SIZE >> 1);
		} else {
			rx_buf.had_overrun = true;
		}
	}

	dma_clear_interrupt_flags(DMA1, DMA_CHANNEL5, isr_flags);
	
}



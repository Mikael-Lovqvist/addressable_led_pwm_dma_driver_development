#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/vector.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>

#include "led_driver.h"
#include "uart_driver.h"

#define SYSTEM_CLOCK_FREQUENCY		24000000
#define BAUD_RATE					1000000

addressable_led_type LED_data[2] = {
	{10, 20, 30},
	{110, 120, 130},
};

volatile addressable_led_pwm_type LED_DMA_buffer[4*24];


addressable_led_driver_instance LED_driver = (addressable_led_driver_instance) {
	.pin_settings = {
		.port = 							GPIOA,
		.mask = 							GPIO6,
		.output_mode = 						GPIO_MODE_OUTPUT_2_MHZ,
		.secondary_remap_setting = 			AFIO_MAPR2_TIM16_REMAP
	},

	.clock_settings = {
		.rcc_mask = 						RCC_GPIOA | RCC_AFIO | RCC_TIM16 | RCC_DMA1,
		.system_clock = 					SYSTEM_CLOCK_FREQUENCY
	},

	.timer_settings = {
		.peripheral = 						TIM16,
		.channel = 							TIM_OC1,
		.output_compare_register =			&TIM16_CCR1,
		.dma_control_register =				&TIM16_DIER,
		.dma_enable_mask = 					TIM_DIER_CC1DE,
		.update_frequency = 				800000,
		.strict_divisor = 					false,
		.have_break_feature = 				true,
		.init_nvic = 						true,
		.dma_irq =							NVIC_DMA1_CHANNEL6_IRQ,
		.dma_irq_priority = 				0,
	},

	.dma_settings = {
		.buffer = 							LED_DMA_buffer,
		.buffer_size = 						sizeof(LED_DMA_buffer),
		.peripheral = 						DMA1,
		.channel = 							DMA_CHANNEL6
	},

	.transfer_settings = {
		.led_buffer = 						LED_data,
		.minimum_tail = 					1,
		.idle_pwm_value = 					0,
		.bit_low_value = 					6,
		.bit_high_value = 					14,
		.channel_order = 					CO_GRB,
		.on_transfer_complete = 			0
	},

};





void configure() {
	rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_HSI_24MHZ]);
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_AFIO);


	gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO_USART1_RX);
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_USART1_TX);

	configure_uart(BAUD_RATE);


}




int main(void) {

	configure();

	if (addressable_led_configure(&LED_driver) != ALE_OK) {
		__asm("bkpt 1");	//If we can't configure we will break or hardfault depending on wether a debugger is attached or not
	}

	if (addressable_led_attach(&LED_driver) != ALE_OK) {
		__asm("bkpt 2");	//If we can't attach hw we will break or hardfault depending on wether a debugger is attached or not
	}


	LED_driver.transfer_settings.led_transfer_count = 1;
	addressable_led_start_transfer(&LED_driver);



	while(true);


	return 0;

}


void hard_fault_handler() {

	while(true) {
		write("uh-oh! ", 7);
		write_flush();
		for(volatile int i=0; i<1000000; i++);
	}

}

void dma1_channel6_isr(void) {
	addressable_led_handle_isr(&LED_driver);
}

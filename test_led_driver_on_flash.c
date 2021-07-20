#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/vector.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/systick.h>
#include <libopencmsis/core_cm3.h>

#include "led_driver.h"
#include "uart_driver.h"

#define SYSTEM_CLOCK_FREQUENCY		24000000
#define BAUD_RATE					1000000

#define LED_Count					216

addressable_led_type LED_data[LED_Count] = {0};

volatile addressable_led_pwm_type LED_DMA_buffer[4*24];

volatile bool ready_for_next = false;



//5 x 7 font, encoded as 5 bytes with bitfields for the column
const char digit_font[] = {0x1c, 0x3e, 0x41, 0x3e, 0x1c, 0x44, 0x7e, 0x7f, 0x40, 0x0, 0x46, 0x63, 0x71, 0x5b, 0x6e, 0x36, 0x63, 0x49, 0x6b, 0x3e, 0x18, 0x54, 0x7e, 0x7f, 0x50, 0x27, 0x4f, 0x49, 0x79, 0x31, 0x38, 0x7e, 0x47, 0x7b, 0x39, 0x47, 0x71, 0x7d, 0x4f, 0x3, 0x36, 0x7f, 0x4b, 0x7f, 0x36, 0x4e, 0x6f, 0x71, 0x3f, 0xe};


addressable_led_driver_instance LED_driver = (addressable_led_driver_instance) {
	.pin_settings = {
		.port = 							GPIOA,
		.mask = 							GPIO6,
		.output_mode = 						GPIO_MODE_OUTPUT_2_MHZ,
		.secondary_remap_setting = 			AFIO_MAPR2_TIM16_REMAP
	},

	.clock_settings = {
		.rcc_list = 						(addressable_led_reg_data[]) {RCC_GPIOA, RCC_AFIO, RCC_TIM16, RCC_DMA1, 0},
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
		.minimum_tail = 					10,
		.idle_pwm_value = 					0,
		.bit_low_value = 					6,
		.bit_high_value = 					14,
		.channel_order = 					CO_GRB,
		.on_transfer_complete = 			0
	},

};




void configure_systick(int frequency) {
	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
	systick_set_reload(SYSTEM_CLOCK_FREQUENCY / frequency - 1);
	systick_interrupt_enable();
	systick_counter_enable();
}


void configure() {
	rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_HSI_24MHZ]);
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_AFIO);


	gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO_USART1_RX);
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_USART1_TX);

	configure_uart(BAUD_RATE);


}

const int column_offset[] = {4, 16, 28, 40, 52, 64, 76, 88, 100, 112, 122, 132, 142, 150, 160, 170, 180, 190, 198, 208};

int hours = 4;
int minutes = 20;


int main(void) {

	configure();


	if (addressable_led_configure(&LED_driver) != ALE_OK) {
		__asm("bkpt 1");	//If we can't configure we will break or hardfault depending on wether a debugger is attached or not
	}

	if (addressable_led_attach(&LED_driver) != ALE_OK) {
		__asm("bkpt 2");	//If we can't attach hw we will break or hardfault depending on wether a debugger is attached or not
	}



	LED_driver.transfer_settings.led_transfer_count = LED_Count;

	configure_systick(10);
	//sys_tick_handler();



	//TODO - figure out a synchronization method where we can update LEDs without risking updating half an LED


/*
	for (int i=0; i<LED_Count; i++) {
		LED_data[i] = (addressable_led_type) {0, 0, 0};
	}



*/

/*
	for (int i=0; i<8; i++) {
		for (int c=0; c<20; c++) {
			LED_data[column_offset[c]+i] = (addressable_led_type) {255-i*30, i*30, 0};
		}
		LED_data[4+i] = (addressable_led_type) {255-i*30, i*30, 0};
		LED_data[16+i] = (addressable_led_type) {255-i*30, i*30, 0};
		LED_data[28+i] = (addressable_led_type) {255-i*30, i*30, 0};
		LED_data[40+i] = (addressable_led_type) {255-i*30, i*30, 0};
		LED_data[52+i] = (addressable_led_type) {255-i*30, i*30, 0};

		LED_data[64+i] = (addressable_led_type) {255-i*30, i*30, 0};
		LED_data[76+i] = (addressable_led_type) {255-i*30, i*30, 0};
		LED_data[88+i] = (addressable_led_type) {255-i*30, i*30, 0};
		LED_data[100+i] = (addressable_led_type) {255-i*30, i*30, 0};
		LED_data[112+i] = (addressable_led_type) {255-i*30, i*30, 0};

		LED_data[122+i] = (addressable_led_type) {255-i*30, i*30, 0};
		LED_data[132+i] = (addressable_led_type) {255-i*30, i*30, 0};
		LED_data[142+i] = (addressable_led_type) {255-i*30, i*30, 0};
		LED_data[150+i] = (addressable_led_type) {255-i*30, i*30, 0};
		LED_data[160+i] = (addressable_led_type) {255-i*30, i*30, 0};

		LED_data[170+i] = (addressable_led_type) {255-i*30, i*30, 0};
		LED_data[180+i] = (addressable_led_type) {255-i*30, i*30, 0};
		LED_data[190+i] = (addressable_led_type) {255-i*30, i*30, 0};
		LED_data[198+i] = (addressable_led_type) {255-i*30, i*30, 0};
		LED_data[208+i] = (addressable_led_type) {255-i*30, i*30, 0};

	}


	//Bottom drawers
	LED_data[120] = (addressable_led_type) {0, 255, 100};
	LED_data[130] = (addressable_led_type) {0, 255, 100};
	LED_data[140] = (addressable_led_type) {0, 255, 100};
	LED_data[158] = (addressable_led_type) {0, 255, 100};
	
	//Four drawers
	LED_data[120+1] = (addressable_led_type) {0, 0, 255};
	LED_data[130+1] = (addressable_led_type) {0, 0, 255};
	LED_data[140+1] = (addressable_led_type) {0, 0, 255};
	LED_data[158+1] = (addressable_led_type) {0, 0, 255};


	//bottom drawers
	LED_data[168] = (addressable_led_type) {0, 255, 100};
	LED_data[178] = (addressable_led_type) {0, 255, 100};
	LED_data[188] = (addressable_led_type) {0, 255, 100};
	LED_data[206] = (addressable_led_type) {0, 255, 100};
	
	//Four drawers
	LED_data[168+1] = (addressable_led_type) {0, 0, 255};
	LED_data[178+1] = (addressable_led_type) {0, 0, 255};
	LED_data[188+1] = (addressable_led_type) {0, 0, 255};
	LED_data[206+1] = (addressable_led_type) {0, 0, 255};*/


	while(true) {	
		//gpio_toggle(GPIOA, GPIO6);

		//For whatever reason, calling the built in div function hardfaults and it also generated
		//a crazy amount of instructions! We only need one call to sdiv/udiv
		
		int digits[4] = {hours / 10, hours % 10, minutes / 10, minutes % 10};

		for (int i=0; i<8; i++) {
			for (int c=0; c<20; c++) {

				//div_t d_c = div(c, 5);	//d_c.quot is digit index and d_c.rem is column
				int d_c_quot = 3 - c / 5;
				int d_c_rem = 4 - c % 5;


				bool fill = digit_font[digits[d_c_quot]*5 + d_c_rem] & (1 << (7-i));
				

				if (fill) {
					LED_data[column_offset[c]+i] = (addressable_led_type) {255-i*30, i*30, 0};
				} else {
					LED_data[column_offset[c]+i] = (addressable_led_type) {0, 1, 2};
				}
			}
		}



		while (!ready_for_next) {
			__WFI();
		}

		ready_for_next = false;
		addressable_led_start_transfer(&LED_driver);


		minutes = (minutes + 1) % 60;
		if (minutes == 0) {
			hours = (hours + 1) % 24;
		}


	}


	return 0;

}


void hard_fault_handler() {

	while(true) {
		for(volatile int i=0; i<1000000; i++);
		write("uh-oh! ", 7);
		write_flush();
	}

}

void dma1_channel6_isr(void) {	
	addressable_led_handle_isr(&LED_driver);	
}


void sys_tick_handler(void) {
	ready_for_next = true;	
}

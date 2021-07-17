#include "led_driver.h"
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>

//Internal macros
#define SELECT_ONE(index, type, ...) (*((type*[]) {__VA_ARGS__})[index])
#define PACK_BYTES(type, ...) *((type*) (char[]) {__VA_ARGS__})


//Prototypes
static void addressable_led_fill_buffer(addressable_led_driver_instance* driver, bool first_half, bool second_half);
static uint32_t addressable_led_convert_nibble(int nibble, int bit_0, int bit_1);




static void addressable_led_fill_buffer(addressable_led_driver_instance* driver, bool first_half, bool second_half) {

	int length;
	volatile uint32_t* dma_buffer_ptr_32;

	void transfer_byte(int byte_index) {
		int value = 0;
		switch (driver->transfer_settings.channel_order) {
			case CO_RGB:	value = SELECT_ONE(byte_index, typeof(driver->state.led_buffer->r), &driver->state.led_buffer->r, &driver->state.led_buffer->g, &driver->state.led_buffer->b); 	break;
			case CO_RBG:	value = SELECT_ONE(byte_index, typeof(driver->state.led_buffer->r), &driver->state.led_buffer->r, &driver->state.led_buffer->b, &driver->state.led_buffer->g); 	break;
			case CO_GRB:	value = SELECT_ONE(byte_index, typeof(driver->state.led_buffer->r), &driver->state.led_buffer->g, &driver->state.led_buffer->r, &driver->state.led_buffer->b); 	break;
			case CO_GBR:	value = SELECT_ONE(byte_index, typeof(driver->state.led_buffer->r), &driver->state.led_buffer->g, &driver->state.led_buffer->b, &driver->state.led_buffer->r); 	break;
			case CO_BRG:	value = SELECT_ONE(byte_index, typeof(driver->state.led_buffer->r), &driver->state.led_buffer->b, &driver->state.led_buffer->r, &driver->state.led_buffer->g); 	break;
			case CO_BGR:	value = SELECT_ONE(byte_index, typeof(driver->state.led_buffer->r), &driver->state.led_buffer->b, &driver->state.led_buffer->g, &driver->state.led_buffer->r); 	break;
		}

		*(dma_buffer_ptr_32++) = addressable_led_convert_nibble(value >> 4, driver->transfer_settings.bit_low_value, driver->transfer_settings.bit_high_value);
		*(dma_buffer_ptr_32++) = addressable_led_convert_nibble(value & 0xF, driver->transfer_settings.bit_low_value, driver->transfer_settings.bit_high_value);
	}
				
	
	if (first_half && second_half) {
		dma_buffer_ptr_32 = (uint32_t*) driver->dma_settings.buffer;
		length = driver->dma_computed.leds_per_buffer;
	} else if (first_half) {
		dma_buffer_ptr_32 = (uint32_t*) driver->dma_settings.buffer;
		length = driver->dma_computed.leds_per_buffer >> 1;
	} else if (second_half) {
 		dma_buffer_ptr_32 = (uint32_t*) (driver->dma_settings.buffer + (driver->dma_computed.actual_buffer_size >> 1));
 		length = driver->dma_computed.leds_per_buffer >> 1;
	} else {
		return;
	}


	for (int i=0; i<length; i++) {
		if (driver->state.remaining_led_elements) {

			for (int b=0; b<3; b++) {
				transfer_byte(b);
			}
			
			driver->state.remaining_led_elements--;
			driver->state.led_buffer++;
		} else {
			if (driver->state.remaining_tail_elements) {
				driver->state.remaining_tail_elements--;
			}
			for (int j=0; j<6; j++) {
				*(dma_buffer_ptr_32++) = 0;
			}
		}
	}	

}



//ARM is little endian so when packing (char[]) {0, 0, 0, 1} into a uint32_t we expect the value 0x01000000
//Because of this, this table calls PACK_BYTES with LSB first
static uint32_t addressable_led_convert_nibble(int nibble, int bit_0, int bit_1) {
	switch (nibble) {
		case 0x0:	return PACK_BYTES(uint32_t, bit_0, bit_0, bit_0, bit_0);
		case 0x1:	return PACK_BYTES(uint32_t, bit_1, bit_0, bit_0, bit_0);
		case 0x2:	return PACK_BYTES(uint32_t, bit_0, bit_1, bit_0, bit_0);
		case 0x3:	return PACK_BYTES(uint32_t, bit_1, bit_1, bit_0, bit_0);
		case 0x4:	return PACK_BYTES(uint32_t, bit_0, bit_0, bit_1, bit_0);
		case 0x5:	return PACK_BYTES(uint32_t, bit_1, bit_0, bit_1, bit_0);
		case 0x6:	return PACK_BYTES(uint32_t, bit_0, bit_1, bit_1, bit_0);
		case 0x7:	return PACK_BYTES(uint32_t, bit_1, bit_1, bit_1, bit_0);
		case 0x8:	return PACK_BYTES(uint32_t, bit_0, bit_0, bit_0, bit_1);
		case 0x9:	return PACK_BYTES(uint32_t, bit_1, bit_0, bit_0, bit_1);
		case 0xA:	return PACK_BYTES(uint32_t, bit_0, bit_1, bit_0, bit_1);
		case 0xB:	return PACK_BYTES(uint32_t, bit_1, bit_1, bit_0, bit_1);
		case 0xC:	return PACK_BYTES(uint32_t, bit_0, bit_0, bit_1, bit_1);
		case 0xD:	return PACK_BYTES(uint32_t, bit_1, bit_0, bit_1, bit_1);
		case 0xE:	return PACK_BYTES(uint32_t, bit_0, bit_1, bit_1, bit_1);
		case 0xF:	return PACK_BYTES(uint32_t, bit_1, bit_1, bit_1, bit_1);
	}
	return 0;
}





void addressable_led_handle_isr(addressable_led_driver_instance* driver) {

	//TODO RECHECK THIS

/*
	if (!(dma_get_interrupt_flag(driver->dma_settings.peripheral, driver->dma_settings.channel, DMA_TCIF | DMA_HTIF))) {
		//Only serve half and full transfer IRQ
		return;
	}

	//We keep track of our state so we can just blindly clear all interrupts relating to this isr
	dma_clear_interrupt_flags(driver->dma_settings.peripheral, driver->dma_settings.channel, DMA_TCIF | DMA_HTIF);

	if (driver->state.state == PBTS_RUN1) {				//First half done
		addressable_led_fill_buffer(driver);
		driver->state.state = PBTS_RUN2;

		if (driver->state.remaining_tail_elements == 0) {
			//We are done with everything so we will turn off circular mode and change state to tail
			//so that the next IRQ can be used to clean up and call the transfer_complete function if set
			DMA_CCR(driver->dma_settings.peripheral, driver->dma_settings.channel) &= ~DMA_CCR_CIRC;
			driver->state.state = PBTS_TAIL;
		}

	} else if (driver->state.state == PBTS_RUN2) {		//Second half done		
		addressable_led_fill_buffer(driver);
		driver->state.state = PBTS_RUN1;

	} else if (driver->state.state == PBTS_TAIL) {
		//All done - turn off IRQs and call completion function if set
		dma_disable_half_transfer_interrupt(driver->dma_settings.peripheral, driver->dma_settings.channel);
		dma_disable_transfer_complete_interrupt(driver->dma_settings.peripheral, driver->dma_settings.channel);
		if (driver->transfer_settings.on_transfer_complete) {
			driver->transfer_settings.on_transfer_complete();
		}
		driver->state.state = PBTS_DONE;
	}
*/

}


//Todo - automatic retransmission
addressable_led_error addressable_led_start_transfer(addressable_led_driver_instance* driver) {

	if (driver->state.state == PBTS_READY) {

		driver->state.remaining_tail_elements = driver->transfer_settings.minimum_tail;
		driver->state.remaining_led_elements = driver->transfer_settings.led_transfer_count;
		driver->state.led_buffer = driver->transfer_settings.led_buffer;

		//Fill both halves of the buffer
		addressable_led_fill_buffer(driver, true, true);

		driver->state.state = PBTS_RUN;


		return ALE_OK;


	} else {
		return ALE_ERROR_UNEXPECTED_STATE;
	}


}


//Attach hardware and configure pins
addressable_led_error addressable_led_attach(addressable_led_driver_instance* driver) {

	if (driver->state.state == PBTS_CONFIGURED) {

		//Start by enabling clocks if an rcc_mask is present
		if (driver->clock_settings.rcc_mask) {
			rcc_periph_clock_enable(driver->clock_settings.rcc_mask);
		}

		//Then initialize GPIO if a pin mask is set
		if (driver->pin_settings.mask) {
			gpio_set_mode(driver->pin_settings.port, driver->pin_settings.output_mode, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, driver->pin_settings.mask);

			if (driver->pin_settings.secondary_remap_setting) {
				gpio_secondary_remap(driver->pin_settings.secondary_remap_setting);
			}

		}

		//Initialize timer peripheral
		timer_set_mode(driver->timer_settings.peripheral, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
		timer_set_period(driver->timer_settings.peripheral, driver->timer_computed.max_pwm);
		timer_set_prescaler(driver->timer_settings.peripheral, 0);
		timer_generate_event(driver->timer_settings.peripheral, TIM_EGR_UG);
		timer_enable_preload(driver->timer_settings.peripheral);

		timer_set_oc_mode(driver->timer_settings.peripheral, driver->timer_settings.channel, TIM_OCM_PWM1);
		timer_enable_oc_preload(driver->timer_settings.peripheral, driver->timer_settings.channel);
		timer_enable_oc_output(driver->timer_settings.peripheral, driver->timer_settings.channel);
		timer_set_oc_value(driver->timer_settings.peripheral, driver->timer_settings.channel, driver->transfer_settings.idle_pwm_value);

		timer_set_dma_on_update_event(driver->timer_settings.peripheral);		
		timer_enable_counter(driver->timer_settings.peripheral);

		if (driver->timer_settings.have_break_feature) {
			timer_enable_break_main_output(driver->timer_settings.peripheral);
		}

		if (driver->timer_settings.init_nvic) {
			nvic_set_priority(driver->timer_settings.dma_irq, driver->timer_settings.dma_irq_priority);
			nvic_enable_irq(driver->timer_settings.dma_irq);
		}


		driver->state.state = PBTS_READY;
		return ALE_OK;

	} else {
		return ALE_ERROR_UNEXPECTED_STATE;
	}

}



//Compute settings and init states
addressable_led_error addressable_led_configure(addressable_led_driver_instance* driver) {

	//Calculate max PWM value
	if (!driver->timer_settings.update_frequency) {
		return ALE_ERROR_MISSING_UPDATE_FREQUENCY;
	}
	if (!driver->clock_settings.system_clock) {
		return ALE_ERROR_MISSING_SYSTEM_CLOCK;
	}

	driver->timer_computed.max_pwm = (driver->clock_settings.system_clock / driver->timer_settings.update_frequency) - 1;

	driver->timer_computed.frequency_error = (driver->timer_computed.max_pwm + 1) * driver->timer_settings.update_frequency - driver->timer_settings.update_frequency;
	if (driver->timer_settings.strict_divisor && driver->timer_computed.frequency_error) {
		return ALE_ERROR_INDIVISABLE_FREQUENCY;
	}

	//Check bounds of transfer settings
	if ((	driver->transfer_settings.idle_pwm_value > driver->timer_computed.max_pwm)
			|| (driver->transfer_settings.bit_low_value > driver->timer_computed.max_pwm)
			|| (driver->transfer_settings.bit_high_value > driver->timer_computed.max_pwm)
		) {
		return ALE_ERROR_PWM_VALUES_OUT_OF_BOUNDS;
	}

	//Check DMA buffer settings
	driver->dma_computed.leds_per_buffer = driver->dma_settings.buffer_size / 24;
	if (!driver->dma_computed.leds_per_buffer) {
		return ALE_ERROR_DMA_BUFFER_TOO_SMALL;
	}


	driver->dma_computed.actual_buffer_size = driver->dma_computed.leds_per_buffer * 24;

	//Check that we have all pointers we need
	if (!driver->dma_settings.buffer) {
		return ALE_ERROR_MISSING_DMA_BUFFER;
	}

	if (!driver->timer_settings.output_compare_register) {
		return ALE_ERROR_MISSING_OUTPUT_COMPARE_REGISTER;
	}

	if (!driver->timer_settings.dma_control_register) {
		return ALE_ERROR_MISSING_DMA_CONTROL_REGISTER;
	}

	if (!driver->timer_settings.dma_enable_mask) {
		return ALE_ERROR_MISSING_DMA_ENABLE_MASK;
	}

	//Initialize state
	driver->state.state = PBTS_CONFIGURED;
	driver->state.error = ALE_OK;

	return ALE_OK;
	

}

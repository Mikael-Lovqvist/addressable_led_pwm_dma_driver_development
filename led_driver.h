#ifndef __LED_DRIVER_H__
#define __LED_DRIVER_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>



//addressable_led_reg_data are for data that goes into register (does not have to be volatile)
typedef uint32_t 								addressable_led_reg_data;	
//addressable_led_reg_ptr is the actual data type for pointing to a register
typedef volatile addressable_led_reg_data* 		addressable_led_reg_ptr;
typedef void(									*addressable_led_callback_type
	)(void);
typedef uint8_t 								addressable_led_pwm_type;
typedef struct addressable_led_type {
	uint8_t	r;
	uint8_t g;
	uint8_t b;
}												addressable_led_type;

typedef enum addressable_led_channel_order {
	CO_RGB,
	CO_RBG,
	CO_GRB,
	CO_GBR,
	CO_BRG,
	CO_BGR,
}												addressable_led_channel_order;

typedef enum addressable_led_transfer_state {
	PBTS_UNINITIALIZED,
	PBTS_CONFIGURED,			//Driver is configured but not attached
	PBTS_READY,					//Driver is attached and ready to use
	PBTS_RUN,
	PBTS_TAIL,
	PBTS_DONE,
	PBTS_ERROR,
}												addressable_led_transfer_state;

typedef enum addressable_led_error {
	ALE_OK,

	ALE_ERROR_DMA_BUFFER_TOO_SMALL,
	ALE_ERROR_INDIVISABLE_FREQUENCY,			//Strict division could not be satisfied
	ALE_ERROR_MISSING_UPDATE_FREQUENCY,
	ALE_ERROR_MISSING_SYSTEM_CLOCK,
	ALE_ERROR_MISSING_DMA_BUFFER,
	ALE_ERROR_MISSING_OUTPUT_COMPARE_REGISTER,
	ALE_ERROR_MISSING_DMA_CONTROL_REGISTER,
	ALE_ERROR_MISSING_DMA_ENABLE_MASK,
	ALE_ERROR_PWM_VALUES_OUT_OF_BOUNDS,	
	ALE_ERROR_UNEXPECTED_STATE,
	
}												addressable_led_error;


/*
	Everything that is configuration time pointers we can check
	Same with masks
	Peripheral IDs might be 0 so we can't check those

*/

typedef struct addressable_led_driver_instance {

	//User settings - should be set before initializing the driver
	struct pin_settings {
		addressable_led_reg_data 				port;
		addressable_led_reg_data 				mask;						//If mask is 0 we will not perform any configuration of gpio
		addressable_led_reg_data				output_mode;
		addressable_led_reg_data				secondary_remap_setting;
	} pin_settings;

	struct clock_settings {
		const addressable_led_reg_data*			rcc_list;
		int										system_clock;
	} clock_settings;

	struct timer_settings {
		addressable_led_reg_data				peripheral;
		addressable_led_reg_data				channel;
		addressable_led_reg_ptr					output_compare_register;
		addressable_led_reg_ptr					dma_control_register;
		addressable_led_reg_data				dma_enable_mask;
		int										update_frequency;
		bool									strict_divisor;
		bool									have_break_feature;		
		bool 									init_nvic;
		addressable_led_reg_data 				dma_irq;
		addressable_led_reg_data 				dma_irq_priority;


	} timer_settings;

	struct dma_settings {
		volatile addressable_led_pwm_type*		buffer;
		size_t									buffer_size;
		addressable_led_reg_data				peripheral;
		addressable_led_reg_data				channel;
	} dma_settings;

	struct transfer_settings {
		const addressable_led_type*				led_buffer;
		size_t									led_transfer_count;

		int										minimum_tail;		//Should make note explaining why 1 or a much higher value are two good options

		addressable_led_pwm_type 				idle_pwm_value;
		addressable_led_pwm_type 				bit_low_value;
		addressable_led_pwm_type 				bit_high_value;
		
		addressable_led_channel_order			channel_order;

		addressable_led_callback_type			on_transfer_complete;
	} transfer_settings;

	//Here are things computed during configuration
	//They should not normally be changed by the user
	struct dma_computed {
		size_t									actual_buffer_size;		//Each LED needs 24 bits so actual_buffer_size will be divisble by 24
		size_t									leds_per_buffer;
	} dma_computed;

	struct timer_computed {
		addressable_led_pwm_type				max_pwm;
		int 									frequency_error;
	} timer_computed;

	//Internal state
	volatile struct state {
		enum addressable_led_transfer_state 	state;
		const addressable_led_type*				led_buffer;

		int										remaining_led_elements;
		int										remaining_tail_elements;

		addressable_led_error					error;
		
	} state;

}												addressable_led_driver_instance;



//Prototypes

void addressable_led_handle_isr(addressable_led_driver_instance* driver);
addressable_led_error addressable_led_configure(addressable_led_driver_instance* driver);
addressable_led_error addressable_led_attach(addressable_led_driver_instance* driver);
addressable_led_error addressable_led_start_transfer(addressable_led_driver_instance* driver);

#endif
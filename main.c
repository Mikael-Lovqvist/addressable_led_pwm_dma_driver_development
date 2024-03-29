#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/nvic.h>


#define FREQ  100000
#define MAX_PWM ((24000000 / FREQ)-1)


#define half(v) (v >> 1)


enum pwm_buffer_transfer_state {
	PBTS_IDLE,
	PBTS_RUN1,	//First half
	PBTS_RUN2,	//Second half
	PBTS_TAIL,
	PBTS_DONE,
};

typedef uint32_t register_content;						//register_content are for data that goes into register (does not have to be volatile)
typedef volatile register_content register_type;		//register_type is the actual data type for registers (has to be volatile)
typedef void(*callback_type)(void);

struct pwm_buffer_transfer {
	enum pwm_buffer_transfer_state state;
	const char* input_buffer;
	int remaining_elements;
	int dma_buffer_length;
	int remaining_tail_elements;
	register_content dma_peripheral;
	register_content dma_channel;
	register_type* peripheral_address;
	volatile char* dma_buffer;
	register_type* dma_control_register;
	register_content dma_begin_transfer;
	callback_type on_transfer_complete;
};

void transfer_pwm_buffer(
	volatile struct pwm_buffer_transfer* transfer, const char* input_buffer, int tail_elements, int size, int dma_length, 
	register_content dma_peripheral, register_content dma_channel, register_type* peripheral_address, volatile char* dma_buffer, 
	register_type* dma_control_register, register_content dma_begin_transfer, callback_type on_transfer_complete);


const char source_buffer[22] = {100, 200, 100, 10, 50, 100, 200, 20, 100, 20, 20, 20, 100, 150, 20, 5, 100, 200, 200, 10, 100, 5};
volatile char test_buffer[16] = {0};
volatile struct pwm_buffer_transfer test_transfer = {0};


void init_rcc() {
	rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_HSI_24MHZ]);
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_AFIO);
	rcc_periph_clock_enable(RCC_TIM16);
	rcc_periph_clock_enable(RCC_DMA1);
	rcc_periph_clock_enable(RCC_USART1);
}


#define TRIGGER_PIN__port 	GPIOA
#define TRIGGER_PIN__pin 	GPIO7

#define local_gpio_set_mode(pin, mode, config) gpio_set_mode(pin##__port, mode, config, pin##__pin)
#define local_gpio_clear(pin) gpio_clear(pin##__port, pin##__pin)
#define local_gpio_toggle(pin) gpio_toggle(pin##__port, pin##__pin)
#define local_gpio_set(pin) gpio_set(pin##__port, pin##__pin)

void init_gpio() {
	//LED data out
	//Remap to PA6
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO6);	
	gpio_secondary_remap(AFIO_MAPR2_TIM16_REMAP);
	

	//Trigger pin for scoping
	local_gpio_clear(TRIGGER_PIN);	
	local_gpio_set_mode(TRIGGER_PIN, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL);


}




void setup_pwm(uint32_t timer_peripheral, enum tim_oc_id channel, int reload) {

	timer_set_mode(timer_peripheral, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
	timer_set_period(timer_peripheral, reload);
	timer_set_prescaler(timer_peripheral, 0);
	timer_generate_event(timer_peripheral, TIM_EGR_UG);
	timer_enable_preload(timer_peripheral);

	timer_set_oc_mode(timer_peripheral, channel, TIM_OCM_PWM1);
	timer_enable_oc_preload(timer_peripheral, channel);
	timer_enable_oc_output(timer_peripheral, channel);
	timer_set_oc_value(timer_peripheral, channel, 0);

	timer_set_dma_on_update_event(timer_peripheral);

	timer_enable_counter(timer_peripheral);
}


void init_systick(int frequency) {
	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
	systick_set_reload(24000000 / frequency - 1);
	systick_interrupt_enable();
	systick_counter_enable();
}

void init_nvic() {
	nvic_set_priority(NVIC_DMA1_CHANNEL6_IRQ, 0);
	nvic_enable_irq(NVIC_DMA1_CHANNEL6_IRQ);
}

int main() {
	init_rcc();
	init_gpio();
	init_nvic();
	setup_pwm(TIM16, TIM_OC1, MAX_PWM);
	timer_set_oc_value(TIM16, TIM_OC1, 0);
	timer_enable_break_main_output(TIM16);


	init_systick(1000);



	while(1);	//spinlock forever and ever and ever ...

	return 0;

}

void pwm_fill_buffer(volatile struct pwm_buffer_transfer* transfer, int offset) {
	
	volatile char* dma_buffer_ptr = transfer->dma_buffer + offset;
	for (int i=0; i<half(transfer->dma_buffer_length); i++) {
		if (transfer->remaining_elements) {
			dma_buffer_ptr[i] = *(transfer->input_buffer++);
			transfer->remaining_elements--;
		} else if (transfer->remaining_tail_elements) {
			transfer->remaining_tail_elements--;
			dma_buffer_ptr[i] = 0;
		} else {
			dma_buffer_ptr[i] = 0;
		}
	}

}

void pwm_transfer_isr(volatile struct pwm_buffer_transfer* transfer) {

	if (!(dma_get_interrupt_flag(transfer->dma_peripheral, transfer->dma_channel, DMA_TCIF | DMA_HTIF))) {
		//Only serve half and full transfer IRQ
		return;
	}

	//We keep track of our state so we can just blindly clear all interrupts relating to this isr
	dma_clear_interrupt_flags(transfer->dma_peripheral, transfer->dma_channel, DMA_TCIF | DMA_HTIF);

	if (transfer->state == PBTS_RUN1) {				//First half done
		pwm_fill_buffer(transfer, half(transfer->dma_buffer_length));
		transfer->state = PBTS_RUN2;

		if (transfer->remaining_tail_elements == 0) {
			//We are done with everything so we will turn off circular mode and change state to tail
			//so that the next IRQ can be used to clean up and call the transfer_complete function if set
			DMA_CCR(transfer->dma_peripheral, transfer->dma_channel) &= ~DMA_CCR_CIRC;
			transfer->state = PBTS_TAIL;
		}

	} else if (transfer->state == PBTS_RUN2) {		//Second half done
		pwm_fill_buffer(transfer, 0);
		transfer->state = PBTS_RUN1;

	} else if (transfer->state == PBTS_TAIL) {
		//All done - turn off IRQs and call completion function if set
		dma_disable_half_transfer_interrupt(transfer->dma_peripheral, transfer->dma_channel);
		dma_disable_transfer_complete_interrupt(transfer->dma_peripheral, transfer->dma_channel);
		if (transfer->on_transfer_complete) {
			transfer->on_transfer_complete();
		}
		transfer->state = PBTS_DONE;
	}

}


void transfer_pwm_buffer(
	volatile struct pwm_buffer_transfer* transfer, const char* input_buffer, int tail_elements, int size, int dma_length, 
	register_content dma_peripheral, register_content dma_channel, register_type* peripheral_address, volatile char* dma_buffer, 
	register_type* dma_control_register, register_content dma_begin_transfer, callback_type on_transfer_complete) {

	*transfer = (struct pwm_buffer_transfer) {
		.state = PBTS_RUN1,
		.input_buffer = input_buffer,
		.remaining_elements = size,
		.dma_buffer_length = dma_length,
		.dma_channel = dma_channel,
		.peripheral_address = peripheral_address,
		.dma_peripheral = dma_peripheral,
		.dma_buffer = dma_buffer,
		.remaining_tail_elements = tail_elements,
		.on_transfer_complete = on_transfer_complete,
		.dma_begin_transfer = dma_begin_transfer,
		.dma_control_register = dma_control_register,
	};

	dma_channel_reset(dma_peripheral, dma_channel);

	dma_set_peripheral_address(dma_peripheral, dma_channel, (register_content) peripheral_address);
	dma_set_memory_address(dma_peripheral, dma_channel, (register_content) dma_buffer);
	dma_set_number_of_data(dma_peripheral, dma_channel, dma_length);
	dma_set_read_from_memory(dma_peripheral, dma_channel);
	dma_enable_memory_increment_mode(dma_peripheral, dma_channel);
	dma_set_peripheral_size(dma_peripheral, dma_channel, DMA_CCR_PSIZE_16BIT);	//Assuming 16 bit for now
	dma_set_memory_size(dma_peripheral, dma_channel, DMA_CCR_MSIZE_8BIT);
	dma_set_priority(dma_peripheral, dma_channel, DMA_CCR_PL_VERY_HIGH);
	dma_enable_circular_mode(dma_peripheral, dma_channel);

	dma_enable_half_transfer_interrupt(dma_peripheral, dma_channel);
	dma_enable_transfer_complete_interrupt(dma_peripheral, dma_channel);
	dma_enable_channel(dma_peripheral, dma_channel);	

	pwm_fill_buffer(transfer, 0);

	//Begin transfer
	*dma_control_register |= dma_begin_transfer;
}

void turn_off_trigger_pin() {
	local_gpio_clear(TRIGGER_PIN);
}

void turn_on_trigger_pin() {
	local_gpio_set(TRIGGER_PIN);
}

void run_dma_experiment() {

	turn_on_trigger_pin();
	transfer_pwm_buffer(
		&test_transfer, 					//Transfer object
		source_buffer, 						//Source buffer
		20, 								//Minimum tail elements
		22, 								//Source buffer elements
		16, 								//DMA buffer length
		DMA1, 								//DMA peripheral
		DMA_CHANNEL6, 						//DMA channel
		&TIM16_CCR1, 						//Timer register
		test_buffer, 						//DMA buffer
		&TIM16_DIER, 						//Timer DMA control register
		TIM_DIER_CC1DE,						//Timer DMA enable value
		turn_off_trigger_pin				//Callback when transfer is done
	);

}



void sys_tick_handler(void) {
	run_dma_experiment();
}


void dma1_channel6_isr(void) {
	pwm_transfer_isr(&test_transfer);
}

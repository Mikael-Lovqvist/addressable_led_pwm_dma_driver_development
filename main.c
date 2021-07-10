#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/nvic.h>


#define FREQ  24000
#define MAX_PWM ((24000000 / FREQ)-1)

void init_rcc() {
	rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_HSI_24MHZ]);
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_AFIO);
	rcc_periph_clock_enable(RCC_TIM16);
	rcc_periph_clock_enable(RCC_DMA1);
	rcc_periph_clock_enable(RCC_USART1);
}


void init_gpio() {
	//LED data out
	//Remap to PA6
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO6);	
	gpio_secondary_remap(AFIO_MAPR2_TIM16_REMAP);
	

	//Trigger pin for scoping
	gpio_clear(GPIOA, GPIO7);
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO7);


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

	timer_enable_counter(timer_peripheral);
}


int main() {
	init_rcc();
	init_gpio();
	setup_pwm(TIM16, TIM_OC1, MAX_PWM);
	timer_set_oc_value(TIM16, TIM_OC1, MAX_PWM >> 1);	//50% duty for initial testing
	timer_enable_break_main_output(TIM16);



	while(1);	//spinlock forever and ever and ever ...

	return 0;

}
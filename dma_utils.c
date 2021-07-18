#include "dma_utils.h"
#include <libopencm3/stm32/dma.h>

uint32_t dma_get_interrupt_flags(uint32_t dma, uint8_t stream) {
	return (DMA_ISR(dma) >> DMA_FLAG_OFFSET(stream)) & DMA_FLAGS;
}


void dma_disable_circular_mode(uint32_t dma, uint8_t stream) {
	DMA_CCR(dma, stream) &= ~DMA_CCR_CIRC;
}
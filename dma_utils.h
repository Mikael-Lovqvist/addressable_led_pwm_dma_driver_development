#ifndef _DMA_UTILS_H_
#define _DMA_UTILS_H_

#include <stdint.h>

uint32_t dma_get_interrupt_flags(uint32_t dma, uint8_t stream);
void dma_disable_circular_mode(uint32_t dma, uint8_t stream);

#endif
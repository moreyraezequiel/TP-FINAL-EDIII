/*
 * irq_dispatch.c - Único lugar donde se definen los handlers de interrupción.
 *
 * Cada handler reconoce y limpia la causa y avisa al módulo dueño. No se promedia, no se
 * imprime ni se controla nada acá: ese trabajo va en el bucle principal (ARCHITECTURE.md, 6).
 */

#include "board_config.h"
#include "excitation.h"
#include "sampling.h"
#include "timebase.h"

void SysTick_Handler(void) {
    if (timebase_tick()) { /* pasó 1 s exacto */
        excitation_on_second();
        sampling_on_second();
    }
}

void DMA_IRQHandler(void) {
    if (GPDMA_IntGetStatus(GPDMA_INTTC, DMA_CANAL_EXCITACION) == SET) {
        GPDMA_ClearIntPending(GPDMA_CLR_INTTC, DMA_CANAL_EXCITACION);
        excitation_on_dma_tc();
    }
    if (GPDMA_IntGetStatus(GPDMA_INTTC, DMA_CANAL_MUESTREO) == SET) {
        GPDMA_ClearIntPending(GPDMA_CLR_INTTC, DMA_CANAL_MUESTREO);
        sampling_on_dma_tc();
    }
}

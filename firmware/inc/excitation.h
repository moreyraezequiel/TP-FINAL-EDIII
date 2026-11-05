/*
 * excitation.h - Excitación del primario: seno por el DAC (P0.26) alimentado por DMA.
 * Dueño de: DAC, su contador y el canal DMA_CANAL_EXCITACION.
 *
 * Señal en el DAC: EXCITACION_HZ, 219 cuentas pico (~0,5 V RMS) centrada en ~VREF/2.
 * La etapa externa de ganancia ~2 la lleva a 1 V RMS en el primario (ARCHITECTURE.md, 5.1).
 */

#ifndef EXCITATION_H
#define EXCITATION_H

#include <stdint.h>

#include "lpc_types.h"

/* Configura el DAC y su DMA y deja la señal corriendo sola. Requiere board_init() antes. */
Status excitation_start(void);

/* Ciclos completos del seno medidos en el último segundo (esperado: EXCITACION_HZ). */
uint32_t excitation_cycles_per_second(void);

/* Solo para irq_dispatch.c */
void excitation_on_dma_tc(void); /* terminó una pasada de la tabla */
void excitation_on_second(void); /* pasó 1 s exacto */

#endif /* EXCITATION_H */

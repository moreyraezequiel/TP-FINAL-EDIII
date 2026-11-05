/*
 * sampling.h - Muestreo del detector: ADC disparado por Timer0, con DMA en bloques.
 * Dueño de: Timer0 (MAT0.1), el ADC (canal AD0.0, P0.23) y el canal DMA_CANAL_MUESTREO.
 *
 * El ADC convierte a ~MUESTREO_HZ sin intervención de la CPU. El DMA llena dos buffers de
 * MUESTREO_BLOQUE muestras que se alternan (ping-pong): mientras se llena uno, el programa
 * procesa el otro. Cada palabra del bloque es el registro ADGDR crudo (resultado en los bits
 * 15:4, DONE en el 31, OVERRUN en el 30); measurement.c se encarga de interpretarlo.
 */

#ifndef SAMPLING_H
#define SAMPLING_H

#include <stdint.h>

#include "lpc_types.h"

/* Configura Timer0 (frenado), el ADC y su DMA. Todavía no hay conversiones.
 * Requiere board_init() antes. */
Status sampling_init(void);

/* Larga el timer: desde acá el ADC convierte. Llamarlo después de configurar todo. */
void sampling_start(void);

/* Devuelve el último bloque completo, o NULL si no hay uno nuevo. El bloque es válido hasta
 * que se complete el siguiente (8 ms): procesarlo antes de volver a llamar. */
const volatile uint32_t* sampling_take_block(void);

/* Bloques completos en el último segundo (esperado: ~125). */
uint32_t sampling_blocks_per_second(void);

/* Bloques que se completaron antes de que el anterior fuera retirado. */
uint32_t sampling_lost_blocks(void);

/* Solo para irq_dispatch.c */
void sampling_on_dma_tc(void); /* se completó un bloque */
void sampling_on_second(void); /* pasó 1 s exacto */

#endif /* SAMPLING_H */

/*
 * simulator.h - Simulador del sensor, para probar sin bobinas (MODO_SIMULADOR en board_config.h).
 * Dueño de: el DAC, en lugar de excitation.
 *
 * El DAC saca una tensión continua que sube y baja despacio, como la salida del detector
 * cuando el núcleo va de una punta a la otra. Uniendo J6-18 (DAC) con J6-15 (ADC), el resto
 * de la cadena (muestreo, promedio, calibración) trabaja igual que con el sensor real.
 */

#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <stdint.h>

/* Configura el DAC sin DMA y lo deja en el nivel más bajo. */
void simulator_start(void);

/* Actualiza el nivel según el tiempo. Llamar seguido desde el bucle principal. */
void simulator_update(uint32_t ahora_ms);

/* Nivel que debería leer el ADC (4 x código del DAC, sin el corrimiento de la placa). */
uint16_t simulator_expected_level(void);

#endif /* SIMULATOR_H */

/*
 * timebase.h - Reloj de milisegundos con SysTick.
 * Dueño de: SysTick.
 */

#ifndef TIMEBASE_H
#define TIMEBASE_H

#include <stdbool.h>
#include <stdint.h>

/* Arranca SysTick con una interrupción cada 1 ms. */
void timebase_init(void);

/* Milisegundos desde el arranque. */
uint32_t timebase_ms(void);

/* Solo para SysTick_Handler (irq_dispatch.c): suma 1 ms y devuelve true cada 1 s exacto. */
bool timebase_tick(void);

#endif /* TIMEBASE_H */

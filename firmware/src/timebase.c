/*
 * timebase.c - Reloj de milisegundos con SysTick.
 */

#include "timebase.h"

#include "lpc17xx_systick.h"

static volatile uint32_t ms;

void timebase_init(void) {
    SYSTICK_InternalInit(1); /* interrupción cada 1 ms */
    SYSTICK_IntCmd(ENABLE);
    SYSTICK_Cmd(ENABLE);
}

uint32_t timebase_ms(void) {
    return ms;
}

bool timebase_tick(void) {
    ms++;
    return (ms % 1000u) == 0;
}

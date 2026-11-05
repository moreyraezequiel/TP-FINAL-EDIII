/*
 * board_config.c - Inicialización común de la placa.
 */

#include "board_config.h"

void board_init(void) {
    SystemCoreClockUpdate();

    GPIO_SetDir(LED_PUERTO, LED_MASCARA, GPIO_OUTPUT);

    /* GPDMA_Init resetea TODOS los canales: va acá, una sola vez, antes que cualquier módulo
     * configure el suyo. Llamarlo de nuevo cortaría la excitación o el muestreo. */
    GPDMA_Init();
    NVIC_EnableIRQ(DMA_IRQn);
}

void board_led_toggle(void) {
    GPIO_TogglePins(LED_PUERTO, LED_MASCARA);
}

/*
 * main.c - Arranque y bucle principal del TP (medición y control de posición con LVDT).
 *
 * Estado actual: excitación del primario (DAC + DMA) y muestreo del detector (ADC + Timer0 +
 * DMA) funcionando. Calibración, servo, control y serie todavía no están implementados.
 *
 * Para mirar en el debugger (pausar antes de leer, pestaña Expressions):
 *   diag             frecuencias medidas, bloques perdidos y errores acumulados
 *   ultima_medicion  promedio, extremos y tensión del último bloque
 *   ultimo_bloque    las 64 muestras del último bloque
 */

#include <stddef.h>

#include "board_config.h"
#include "excitation.h"
#include "measurement.h"
#include "sampling.h"
#include "timebase.h"

typedef struct {
    Status arranque;              /* ERROR si falló la configuración de algún DMA */
    uint32_t ciclos_por_segundo;  /* excitación: esperado 1000 */
    uint32_t bloques_por_segundo; /* muestreo: esperado ~125 */
    uint32_t bloques_procesados;
    uint32_t bloques_perdidos;    /* el bucle no llegó a retirar un bloque a tiempo */
    uint32_t muestras_invalidas;  /* acumuladas desde el arranque */
    uint32_t muestras_overrun;    /* acumuladas desde el arranque */
} diag_t;

volatile diag_t diag;
volatile medicion_t ultima_medicion;
uint16_t ultimo_bloque[MUESTREO_BLOQUE];

int main(void) {
    board_init();    /* reloj, LED y GPDMA (una sola vez) */
    timebase_init(); /* SysTick cada 1 ms */

    /* Orden de arranque: el timer queda frenado dentro de sampling_init y recién se larga
     * cuando todo está configurado, así no hay conversiones a medio configurar. */
    Status ok = sampling_init();
    if (ok == SUCCESS) {
        ok = excitation_start();
    }
    diag.arranque = ok;
    sampling_start();

    const uint32_t periodo_led_ms = (ok == SUCCESS) ? 500u : 100u; /* rápido = falló algo */
    uint32_t proximo_led          = 0;

    while (1) {
        const volatile uint32_t* bloque = sampling_take_block();
        if (bloque != NULL) {
            medicion_t m;
            measurement_process(bloque, MUESTREO_BLOQUE, timebase_ms(), &m, ultimo_bloque);
            ultima_medicion = m;
            diag.bloques_procesados++;
            diag.muestras_invalidas += m.muestras_invalidas;
            diag.muestras_overrun += m.muestras_overrun;
        }

        diag.ciclos_por_segundo  = excitation_cycles_per_second();
        diag.bloques_por_segundo = sampling_blocks_per_second();
        diag.bloques_perdidos    = sampling_lost_blocks();

        if ((int32_t)(timebase_ms() - proximo_led) >= 0) {
            proximo_led += periodo_led_ms;
            board_led_toggle();
        }
    }
}

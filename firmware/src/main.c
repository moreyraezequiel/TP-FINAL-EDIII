/*
 * main.c - Arranque y bucle principal del TP (medición y control de posición con LVDT).
 *
 * Estado actual: excitación del primario (DAC + DMA), muestreo del detector (ADC + Timer0 +
 * DMA), promedio por bloque y conversión a mm con una tabla de calibración. Servo, control y
 * comunicación serie todavía no están implementados.
 *
 * Con MODO_SIMULADOR = 1 (board_config.h) el DAC simula al detector con una rampa lenta, así
 * se prueba toda la cadena con un cable de J6-18 a J6-15, sin bobinas.
 *
 * Para mirar en el debugger (pausar antes de leer, pestaña Expressions):
 *   diag             estado del arranque, frecuencias medidas y errores acumulados
 *   ultima_medicion  promedio, extremos y tensión del último bloque
 *   posicion         posición en mm, estado de la calibración y validez
 *   ultimo_bloque    las 64 muestras del último bloque
 */

#include <stddef.h>

#include "board_config.h"
#include "calibration.h"
#include "calibration_table.h"
#include "excitation.h"
#include "measurement.h"
#include "sampling.h"
#include "simulator.h"
#include "timebase.h"

typedef struct {
    Status arranque;               /* ERROR si falló la configuración de algún DMA */
    cal_error_tabla_t calibracion; /* resultado de cargar la tabla al arrancar */
    bool tabla_de_ejemplo;         /* true = la tabla no viene de medir el sensor */
    bool modo_simulador;
    uint32_t ciclos_por_segundo;   /* excitación: 1000 (0 en modo simulador) */
    uint32_t bloques_por_segundo;  /* muestreo: ~125 */
    uint32_t bloques_procesados;
    uint32_t bloques_perdidos;     /* el bucle no llegó a retirar un bloque a tiempo */
    uint32_t muestras_invalidas;   /* acumuladas desde el arranque */
    uint32_t muestras_overrun;     /* acumuladas desde el arranque */
    uint16_t simulador_nivel_esperado; /* modo simulador: lo que debería leer el ADC */
} diag_t;

typedef struct {
    uint32_t timestamp_ms;
    float posicion_mm;      /* vale solo si valida es true */
    cal_resultado_t estado; /* CAL_OK, CAL_SIN_CALIBRAR o CAL_FUERA_DE_RANGO */
    bool valida;            /* medición sin errores y dentro del tramo calibrado */
} posicion_t;

volatile diag_t diag;
volatile medicion_t ultima_medicion;
volatile posicion_t posicion;
uint16_t ultimo_bloque[MUESTREO_BLOQUE];

static void procesar(const volatile uint32_t* bloque) {
    medicion_t m;
    measurement_process(bloque, MUESTREO_BLOQUE, timebase_ms(), &m, ultimo_bloque);
    ultima_medicion = m;
    diag.bloques_procesados++;
    diag.muestras_invalidas += m.muestras_invalidas;
    diag.muestras_overrun += m.muestras_overrun;

    posicion_t p = {.timestamp_ms = m.timestamp_ms, .posicion_mm = 0.0f};
    float mm     = 0.0f;
    p.estado     = calibration_level_to_mm(m.adc_promedio, &mm);
    p.valida     = m.valida && (p.estado == CAL_OK);
    if (p.valida) {
        p.posicion_mm = mm;
    }
    posicion = p;
}

int main(void) {
    board_init();    /* reloj, LED y GPDMA (una sola vez) */
    timebase_init(); /* SysTick cada 1 ms */

    diag.modo_simulador   = (MODO_SIMULADOR != 0);
    diag.calibracion      = calibration_load(calibracion_tabla, calibracion_tabla_n);
    diag.tabla_de_ejemplo = calibracion_tabla_es_ejemplo;

    /* Orden de arranque: el timer queda frenado dentro de sampling_init y recién se larga
     * cuando todo está configurado, así no hay conversiones a medio configurar. */
    Status ok = sampling_init();
#if MODO_SIMULADOR
    simulator_start();
#else
    if (ok == SUCCESS) {
        ok = excitation_start();
    }
#endif
    diag.arranque = ok;
    sampling_start();

    const uint32_t periodo_led_ms = (ok == SUCCESS) ? 500u : 100u; /* rápido = falló algo */
    uint32_t proximo_led          = 0;

    while (1) {
#if MODO_SIMULADOR
        simulator_update(timebase_ms());
        diag.simulador_nivel_esperado = simulator_expected_level();
#endif

        const volatile uint32_t* bloque = sampling_take_block();
        if (bloque != NULL) {
            procesar(bloque);
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

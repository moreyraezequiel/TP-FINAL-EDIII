/*
 * simulator.c - Rampa lenta por el DAC que imita al detector con el núcleo moviéndose.
 *
 * Forma: sube de SIM_CODIGO_MIN a SIM_CODIGO_MAX en medio período y baja en el otro medio.
 * Con la tabla de ejemplo de calibration_table.c, los extremos de la rampa quedan afuera del
 * tramo calibrado, así que también se ve el estado CAL_FUERA_DE_RANGO.
 */

#include "simulator.h"

#include "lpc17xx_dac.h"

#define SIM_PERIODO_MS 20000u /* ida y vuelta: 10 s subiendo y 10 s bajando */
#define SIM_CODIGO_MIN 150u   /* ADC ~600 */
#define SIM_CODIGO_MAX 850u   /* ADC ~3400 */

static uint32_t codigo_actual = SIM_CODIGO_MIN;

void simulator_start(void) {
    DAC_Init(); /* P0.26 como AOUT */

    /* Sin contador ni DMA: se escribe el valor directo. Se apaga explícitamente por si quedó
     * configurado de una corrida anterior con la excitación (el debugger no siempre resetea
     * los periféricos). */
    const DAC_CONVERTER_CFG_T dac = {
        .doubleBuffer = DISABLE,
        .dmaCounter   = DISABLE,
        .dmaRequest   = DISABLE,
    };
    DAC_ConfigDAConverterControl(&dac);
    DAC_UpdateValue(codigo_actual);
}

void simulator_update(uint32_t ahora_ms) {
    const uint32_t mitad = SIM_PERIODO_MS / 2u;
    const uint32_t rango = SIM_CODIGO_MAX - SIM_CODIGO_MIN;
    const uint32_t t     = ahora_ms % SIM_PERIODO_MS;

    const uint32_t codigo = (t < mitad) ? SIM_CODIGO_MIN + rango * t / mitad
                                        : SIM_CODIGO_MAX - rango * (t - mitad) / mitad;
    if (codigo != codigo_actual) {
        codigo_actual = codigo;
        DAC_UpdateValue(codigo);
    }
}

uint16_t simulator_expected_level(void) {
    return (uint16_t)(codigo_actual * 4u); /* DAC de 10 bits -> ADC de 12 bits */
}

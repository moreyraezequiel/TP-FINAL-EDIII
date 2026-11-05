/*
 * measurement.c - Promedio, extremos y calidad de un bloque del ADC.
 */

#include "measurement.h"

#include <stddef.h>

#include "board_config.h"
#include "lpc17xx_adc.h"

void measurement_process(const volatile uint32_t* bloque, uint32_t n, uint32_t timestamp_ms,
                         medicion_t* m, uint16_t* copia) {
    uint32_t suma       = 0;
    uint16_t mn         = 4095;
    uint16_t mx         = 0;
    uint32_t invalidas  = 0;
    uint32_t overrun    = 0;

    for (uint32_t i = 0; i < n; i++) {
        const uint32_t w = bloque[i];
        if ((w & ADC_DR_DONE_FLAG) == 0) {
            invalidas++;
        }
        if (w & ADC_DR_OVERRUN_FLAG) {
            overrun++;
        }
        const uint16_t v = (uint16_t)ADC_GDR_RESULT(w);
        if (copia != NULL) {
            copia[i] = v;
        }
        suma += v;
        if (v < mn) {
            mn = v;
        }
        if (v > mx) {
            mx = v;
        }
    }

    m->timestamp_ms       = timestamp_ms;
    m->adc_promedio       = (n > 0) ? (uint16_t)(suma / n) : 0;
    m->adc_min            = mn;
    m->adc_max            = mx;
    m->tension_adc_mV     = ((uint32_t)m->adc_promedio * VREF_MV + 2048u) / 4096u;
    m->muestras_invalidas = invalidas;
    m->muestras_overrun   = overrun;
    m->valida             = (n > 0) && (invalidas == 0) && (overrun == 0);
}

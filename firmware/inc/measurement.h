/*
 * measurement.h - Interpretación de un bloque crudo del ADC.
 * No toca periféricos: recibe el bloque de sampling y devuelve promedio, extremos, tensión,
 * calidad y marca de tiempo (ARCHITECTURE.md, 7.1). No calcula posición ni mueve el servo.
 */

#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t timestamp_ms;       /* cuándo se procesó el bloque */
    uint16_t adc_promedio;       /* cuentas, 0 a 4095 */
    uint16_t adc_min;
    uint16_t adc_max;
    uint32_t tension_adc_mV;     /* promedio en milivolts (usa VREF_MV) */
    uint32_t muestras_invalidas; /* palabras sin DONE en este bloque */
    uint32_t muestras_overrun;   /* resultados pisados antes de leerse */
    bool valida;                 /* true si no hubo inválidas ni overrun */
} medicion_t;

/* Procesa n palabras crudas de ADGDR. Si copia no es NULL, guarda ahí los n resultados de
 * 12 bits (útil para mirar el bloque en el debugger). */
void measurement_process(const volatile uint32_t* bloque, uint32_t n, uint32_t timestamp_ms,
                         medicion_t* m, uint16_t* copia);

#endif /* MEASUREMENT_H */

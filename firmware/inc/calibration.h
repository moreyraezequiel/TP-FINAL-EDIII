/*
 * calibration.h - Conversión de nivel del ADC a posición en mm (ARCHITECTURE.md, 5.4 y 7.1).
 *
 * Usa una tabla de puntos (posición medida con la regla, nivel del ADC en esa posición) e
 * interpola linealmente entre ellos. Solo acepta tablas en las que el nivel siempre sube o
 * siempre baja con la posición, con una sensibilidad mínima en cada tramo. Fuera de la tabla
 * no extrapola: devuelve CAL_FUERA_DE_RANGO. Sin tabla cargada devuelve CAL_SIN_CALIBRAR.
 *
 * No toca periféricos: se puede compilar y probar en la PC (ver tests/).
 */

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <stdbool.h>
#include <stdint.h>

#define CAL_MAX_PUNTOS 32u

/* Cuentas de ADC por mm mínimas en cada tramo. Por debajo, un paso del ADC equivale a más de
 * 1 mm y la posición no es confiable. Ajustar cuando se conozca el ruido real del sensor. */
#define CAL_SENSIBILIDAD_MIN 1.0f

typedef struct {
    float posicion_mm; /* posición real, medida con la regla */
    uint16_t nivel;    /* promedio del ADC en esa posición (cuentas, 0 a 4095) */
} cal_punto_t;

typedef enum {
    CAL_OK = 0,
    CAL_SIN_CALIBRAR,  /* no hay una tabla válida cargada */
    CAL_FUERA_DE_RANGO /* el nivel cae afuera del tramo calibrado */
} cal_resultado_t;

typedef enum {
    CAL_TABLA_OK = 0,
    CAL_TABLA_POCOS_PUNTOS,            /* menos de 2 puntos */
    CAL_TABLA_DEMASIADOS_PUNTOS,       /* más de CAL_MAX_PUNTOS */
    CAL_TABLA_POSICIONES_DESORDENADAS, /* las posiciones no crecen estrictamente */
    CAL_TABLA_NO_MONOTONA,             /* el nivel no sube (o no baja) siempre */
    CAL_TABLA_POCA_SENSIBILIDAD        /* algún tramo con menos de CAL_SENSIBILIDAD_MIN */
} cal_error_tabla_t;

/* Valida y copia la tabla. Si es inválida, el módulo queda sin calibrar. */
cal_error_tabla_t calibration_load(const cal_punto_t* puntos, uint32_t n);

/* Borra la calibración. */
void calibration_clear(void);

bool calibration_is_valid(void);

/* Convierte un nivel en mm. Solo escribe *posicion_mm si devuelve CAL_OK. */
cal_resultado_t calibration_level_to_mm(uint16_t nivel, float* posicion_mm);

/* Extremos del tramo calibrado, para limitar las consignas. false si no hay calibración. */
bool calibration_range_mm(float* min_mm, float* max_mm);

#endif /* CALIBRATION_H */

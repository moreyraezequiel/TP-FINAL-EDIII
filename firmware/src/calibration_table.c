/*
 * calibration_table.c - Tabla de calibración que se carga al arrancar.
 *
 * ATENCIÓN: por ahora es una tabla DE EJEMPLO, pensada para el modo simulador. No viene de
 * medir el sensor. Cuando se mida la curva real (ARCHITECTURE.md, 5.4), reemplazar los puntos
 * por los medidos y poner calibracion_tabla_es_ejemplo en false.
 *
 * Cada punto es la posición real del núcleo en mm (medida con la regla) y el promedio del ADC
 * en esa posición (ultima_medicion.adc_promedio). Las posiciones van en orden creciente y el
 * nivel tiene que subir siempre o bajar siempre: si la curva medida da la vuelta, cargar solo
 * el tramo que va en un solo sentido.
 */

#include "calibration_table.h"

const cal_punto_t calibracion_tabla[] = {
    {.posicion_mm = 0.0f, .nivel = 900},
    {.posicion_mm = 10.0f, .nivel = 1380},
    {.posicion_mm = 20.0f, .nivel = 1880},
    {.posicion_mm = 30.0f, .nivel = 2370},
    {.posicion_mm = 40.0f, .nivel = 2820},
    {.posicion_mm = 50.0f, .nivel = 3200},
};

const uint32_t calibracion_tabla_n = sizeof(calibracion_tabla) / sizeof(calibracion_tabla[0]);

const bool calibracion_tabla_es_ejemplo = true;

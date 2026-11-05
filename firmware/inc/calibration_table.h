/*
 * calibration_table.h - Tabla de calibración que se carga al arrancar.
 */

#ifndef CALIBRATION_TABLE_H
#define CALIBRATION_TABLE_H

#include <stdbool.h>
#include <stdint.h>

#include "calibration.h"

extern const cal_punto_t calibracion_tabla[];
extern const uint32_t calibracion_tabla_n;

/* true mientras la tabla sea de ejemplo y no venga de medir el sensor real. */
extern const bool calibracion_tabla_es_ejemplo;

#endif /* CALIBRATION_TABLE_H */

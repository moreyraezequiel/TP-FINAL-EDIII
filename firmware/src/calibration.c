/*
 * calibration.c - Tabla de calibración e interpolación lineal.
 */

#include "calibration.h"

#include <stddef.h>

static cal_punto_t tabla[CAL_MAX_PUNTOS];
static uint32_t n_puntos; /* 0 = sin calibrar */
static bool creciente;    /* true si el nivel sube cuando sube la posición */

static cal_error_tabla_t validar(const cal_punto_t* p, uint32_t n, bool* sube) {
    if (p == NULL || n < 2u) {
        return CAL_TABLA_POCOS_PUNTOS;
    }
    if (n > CAL_MAX_PUNTOS) {
        return CAL_TABLA_DEMASIADOS_PUNTOS;
    }

    const bool primer_tramo_sube = p[1].nivel > p[0].nivel;
    for (uint32_t i = 1; i < n; i++) {
        const float dpos = p[i].posicion_mm - p[i - 1].posicion_mm;
        if (!(dpos > 0.0f)) { /* también rechaza NaN */
            return CAL_TABLA_POSICIONES_DESORDENADAS;
        }

        const int32_t dniv = (int32_t)p[i].nivel - (int32_t)p[i - 1].nivel;
        if (dniv == 0 || (dniv > 0) != primer_tramo_sube) {
            return CAL_TABLA_NO_MONOTONA;
        }

        const float sensibilidad = (float)(dniv > 0 ? dniv : -dniv) / dpos;
        if (sensibilidad < CAL_SENSIBILIDAD_MIN) {
            return CAL_TABLA_POCA_SENSIBILIDAD;
        }
    }

    *sube = primer_tramo_sube;
    return CAL_TABLA_OK;
}

cal_error_tabla_t calibration_load(const cal_punto_t* puntos, uint32_t n) {
    bool sube = false;
    const cal_error_tabla_t error = validar(puntos, n, &sube);
    if (error != CAL_TABLA_OK) {
        n_puntos = 0; /* una tabla mala no deja una calibración vieja activa */
        return error;
    }

    for (uint32_t i = 0; i < n; i++) {
        tabla[i] = puntos[i];
    }
    creciente = sube;
    n_puntos  = n;
    return CAL_TABLA_OK;
}

void calibration_clear(void) {
    n_puntos = 0;
}

bool calibration_is_valid(void) {
    return n_puntos >= 2u;
}

cal_resultado_t calibration_level_to_mm(uint16_t nivel, float* posicion_mm) {
    if (!calibration_is_valid() || posicion_mm == NULL) {
        return CAL_SIN_CALIBRAR;
    }

    const uint16_t primero = tabla[0].nivel;
    const uint16_t ultimo  = tabla[n_puntos - 1u].nivel;
    const uint16_t minimo  = creciente ? primero : ultimo;
    const uint16_t maximo  = creciente ? ultimo : primero;
    if (nivel < minimo || nivel > maximo) {
        return CAL_FUERA_DE_RANGO;
    }

    /* Buscar el tramo [a, b] que contiene al nivel e interpolar. */
    for (uint32_t i = 1; i < n_puntos; i++) {
        const cal_punto_t a = tabla[i - 1u];
        const cal_punto_t b = tabla[i];
        const bool en_tramo  = creciente ? (nivel <= b.nivel) : (nivel >= b.nivel);
        if (en_tramo) {
            const float fraccion = (float)((int32_t)nivel - (int32_t)a.nivel) /
                                   (float)((int32_t)b.nivel - (int32_t)a.nivel);
            *posicion_mm = a.posicion_mm + fraccion * (b.posicion_mm - a.posicion_mm);
            return CAL_OK;
        }
    }

    *posicion_mm = tabla[n_puntos - 1u].posicion_mm; /* no debería llegar acá */
    return CAL_OK;
}

bool calibration_range_mm(float* min_mm, float* max_mm) {
    if (!calibration_is_valid() || min_mm == NULL || max_mm == NULL) {
        return false;
    }
    *min_mm = tabla[0].posicion_mm;
    *max_mm = tabla[n_puntos - 1u].posicion_mm;
    return true;
}

/*
 * test_calibration.c - Pruebas del módulo de calibración que corren en la PC (sin placa).
 *
 * Desde WSL:   cd firmware/tests && make
 * Termina con "N pruebas, 0 fallas" y código de salida 0 si todo está bien.
 */

#include <stdio.h>

#include "calibration.h"

static int pruebas = 0;
static int fallas  = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        pruebas++;                                                      \
        if (!(cond)) {                                                  \
            fallas++;                                                   \
            printf("FALLA linea %d: %s\n", __LINE__, #cond);            \
        }                                                               \
    } while (0)

static int cerca(float a, float b) {
    const float d = a - b;
    return (d < 0.0f ? -d : d) < 0.001f;
}

/* Misma forma que la tabla de ejemplo: el nivel sube con la posición. */
static const cal_punto_t creciente[] = {
    {0.0f, 900}, {10.0f, 1380}, {20.0f, 1880}, {30.0f, 2370}, {40.0f, 2820}, {50.0f, 3200},
};

/* El nivel baja cuando la posición sube (también puede pasar con el sensor real). */
static const cal_punto_t decreciente[] = {
    {-20.0f, 3000}, {0.0f, 2000}, {25.0f, 1000},
};

static void sin_tabla(void) {
    float mm = 123.0f;
    calibration_clear();
    CHECK(!calibration_is_valid());
    CHECK(calibration_level_to_mm(2000, &mm) == CAL_SIN_CALIBRAR);
    CHECK(mm == 123.0f); /* sin calibración no escribe una posición */
}

static void tablas_invalidas(void) {
    const cal_punto_t desordenada[]   = {{0.0f, 900}, {10.0f, 1400}, {5.0f, 1900}};
    const cal_punto_t pos_repetida[]  = {{0.0f, 900}, {0.0f, 1400}};
    const cal_punto_t no_monotona[]   = {{0.0f, 900}, {10.0f, 1400}, {20.0f, 1200}};
    const cal_punto_t plana[]         = {{0.0f, 900}, {10.0f, 900}};
    const cal_punto_t poco_sensible[] = {{0.0f, 1000}, {10.0f, 1005}}; /* 0,5 cuentas/mm */
    cal_punto_t demasiados[CAL_MAX_PUNTOS + 1u];
    for (unsigned i = 0; i < CAL_MAX_PUNTOS + 1u; i++) {
        demasiados[i].posicion_mm = (float)i;
        demasiados[i].nivel       = (uint16_t)(100u + 50u * i);
    }

    CHECK(calibration_load(creciente, 1) == CAL_TABLA_POCOS_PUNTOS);
    CHECK(calibration_load(NULL, 5) == CAL_TABLA_POCOS_PUNTOS);
    CHECK(calibration_load(demasiados, CAL_MAX_PUNTOS + 1u) == CAL_TABLA_DEMASIADOS_PUNTOS);
    CHECK(calibration_load(desordenada, 3) == CAL_TABLA_POSICIONES_DESORDENADAS);
    CHECK(calibration_load(pos_repetida, 2) == CAL_TABLA_POSICIONES_DESORDENADAS);
    CHECK(calibration_load(no_monotona, 3) == CAL_TABLA_NO_MONOTONA);
    CHECK(calibration_load(plana, 2) == CAL_TABLA_NO_MONOTONA);
    CHECK(calibration_load(poco_sensible, 2) == CAL_TABLA_POCA_SENSIBILIDAD);
    CHECK(calibration_load(demasiados, CAL_MAX_PUNTOS) == CAL_TABLA_OK); /* justo el máximo */

    /* Una carga fallida no deja activa la calibración anterior. */
    CHECK(calibration_load(creciente, 6) == CAL_TABLA_OK);
    CHECK(calibration_load(no_monotona, 3) == CAL_TABLA_NO_MONOTONA);
    CHECK(!calibration_is_valid());
}

static void tabla_creciente(void) {
    float mm = 0.0f;
    float a  = 0.0f;
    float b  = 0.0f;

    CHECK(calibration_load(creciente, 6) == CAL_TABLA_OK);
    CHECK(calibration_is_valid());

    CHECK(calibration_level_to_mm(900, &mm) == CAL_OK && cerca(mm, 0.0f));   /* borde */
    CHECK(calibration_level_to_mm(3200, &mm) == CAL_OK && cerca(mm, 50.0f)); /* borde */
    CHECK(calibration_level_to_mm(1880, &mm) == CAL_OK && cerca(mm, 20.0f)); /* punto */
    CHECK(calibration_level_to_mm(1630, &mm) == CAL_OK && cerca(mm, 15.0f)); /* mitad de tramo */
    CHECK(calibration_level_to_mm(3010, &mm) == CAL_OK && cerca(mm, 45.0f));
    CHECK(calibration_level_to_mm(899, &mm) == CAL_FUERA_DE_RANGO);
    CHECK(calibration_level_to_mm(3201, &mm) == CAL_FUERA_DE_RANGO);
    CHECK(calibration_level_to_mm(0, &mm) == CAL_FUERA_DE_RANGO);
    CHECK(calibration_level_to_mm(4095, &mm) == CAL_FUERA_DE_RANGO);

    CHECK(calibration_range_mm(&a, &b) && cerca(a, 0.0f) && cerca(b, 50.0f));
}

static void tabla_decreciente(void) {
    float mm = 0.0f;

    CHECK(calibration_load(decreciente, 3) == CAL_TABLA_OK);
    CHECK(calibration_level_to_mm(3000, &mm) == CAL_OK && cerca(mm, -20.0f));
    CHECK(calibration_level_to_mm(2500, &mm) == CAL_OK && cerca(mm, -10.0f));
    CHECK(calibration_level_to_mm(2000, &mm) == CAL_OK && cerca(mm, 0.0f));
    CHECK(calibration_level_to_mm(1500, &mm) == CAL_OK && cerca(mm, 12.5f));
    CHECK(calibration_level_to_mm(1000, &mm) == CAL_OK && cerca(mm, 25.0f));
    CHECK(calibration_level_to_mm(999, &mm) == CAL_FUERA_DE_RANGO);
    CHECK(calibration_level_to_mm(3001, &mm) == CAL_FUERA_DE_RANGO);
}

int main(void) {
    sin_tabla();
    tablas_invalidas();
    tabla_creciente();
    tabla_decreciente();

    printf("%d pruebas, %d fallas\n", pruebas, fallas);
    return fallas == 0 ? 0 : 1;
}

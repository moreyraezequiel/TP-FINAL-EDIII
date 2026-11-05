# Pruebas de firmware

Programas de prueba independientes, usados para verificar en la placa cada parte antes de
pasarla a los módulos de `firmware/`. **No forman parte del proyecto:** cada uno tiene su propio
`main`. Para repetir una prueba, pegar el archivo como único `.c` de un proyecto de prueba en
MCUXpresso (vinculado a `CMSISv2p00_LPC17xx` y con `Drivers/inc` en los includes).

- Placa: LPCXpresso LPC1769 rev C.
- IDE: MCUXpresso IDE 25.6.136.
- Drivers: `lib/CMSISv2p00_LPC17xx`, commit `511893e`.
- Fecha: 5/11/2026.
- Conexión para las dos pruebas: un cable de J6-18 (P0.26, salida del DAC) a J6-15 (P0.23,
  entrada del ADC).
- Los valores se leyeron en la pestaña Expressions con el programa en pausa.

## Prueba 1: seno de 1 kHz con DAC + DMA (issue #19)

Archivo: `prueba_seno_dac_dma.c`. Tabla de 32 muestras por DMA con una LLI circular. El ADC
lee la salida del DAC por consulta (polling) y calcula mínimo, máximo, centro y pico a pico.
Una interrupción de fin de bloque del DMA cuenta los ciclos por segundo.

| Medición | Resultado | Esperado |
|---|---|---|
| Mínimo (cuentas) | 1203 | ~1172 |
| Máximo (cuentas) | 2953 | ~2924 |
| Centro (cuentas) | 2078 | ~2048 |
| Pico a pico (cuentas) | 1750 | ~1752 |
| Frecuencia, DACCNTVAL = 781 | 999 Hz | 1000 Hz |
| Frecuencia, DACCNTVAL = 780 | 1000 Hz | 1000 Hz |

- La amplitud es correcta. Hay un corrimiento fijo de unas +30 cuentas en el ADC y el DAC.
- El contador del DAC cuenta desde DACCNTVAL hasta 0 inclusive, así que cada muestra dura
  DACCNTVAL + 1 ciclos de PCLK. Con 781 la frecuencia daba 999,04 Hz; se corrigió a 780
  (1000,32 Hz).

## Prueba 2: ADC por Timer0 + DMA en bloques (issue #22)

Archivo: `prueba_adc_timer_dma.c`. MAT0.1 hace toggle a ~16 kHz y dispara el ADC a 8002,6
muestras/s. El DMA llena dos buffers de 64 muestras que se alternan. Sigue generando el seno de
la prueba 1 como señal de entrada.

| Medición | Resultado | Esperado |
|---|---|---|
| Bloques por segundo | 125 | ~125 |
| Ciclos del seno por segundo | 1000 | 1000 |
| Promedio de un bloque (cuentas) | 2076 | ~2078 |
| Muestras inválidas / overrun / bloques perdidos | 0 / 0 / 0 | 0 / 0 / 0 |

- El bloque muestra 8 valores que se repiten 8 veces, por ejemplo 2245, 2801, 2933, 2563,
  1906, 1354, 1221, 1592. El ADC y el DAC salen del mismo reloj y 8002,6 / 8 = 1000,32 Hz,
  así que el muestreo queda sincronizado con el seno. El promedio no depende de la fase.
- En la primera versión aparecieron 1 muestra inválida y 2 overruns solo al arrancar: el
  Timer0 seguía andando de la corrida anterior mientras se configuraba el ADC. Se resolvió
  configurando el timer frenado primero y largándolo al final.

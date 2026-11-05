# Firmware

Proyecto de MCUXpresso `TP_LVDT` para la LPCXpresso LPC1769. Sigue la división en módulos y el
reparto de recursos de [ARCHITECTURE.md](../ARCHITECTURE.md).

## Estado

| Parte | Módulo | Estado |
|---|---|---|
| Excitación del primario (DAC + DMA) | `excitation` | Funciona en la placa (issue #19) |
| Muestreo del detector (ADC + Timer0 + DMA) | `sampling` | Funciona en la placa (issue #22) |
| Promedio y calidad de cada bloque | `measurement` | Funciona en la placa |
| Conversión de nivel a mm | `calibration` | Funciona con una tabla **de ejemplo**; falta medir la curva real (issue #23) |
| Simulador del sensor para probar sin bobinas | `simulator` | Listo (`MODO_SIMULADOR` en `board_config.h`) |
| Servo, control y serie | - | Pendiente |

La evidencia de las pruebas está en [pruebas/firmware](../pruebas/firmware/README.md).

## Requisitos

- MCUXpresso IDE 25.6.136 (no hace falta ningún SDK).
- LPCXpresso LPC1769 rev B o C. En la rev D cambian el LED y el programador.
- La librería de drivers en [`../lib/CMSISv2p00_LPC17xx`](../lib/README.md), incluida en el repo.

## Cómo importarlo en MCUXpresso

1. Si en el workspace ya hay un proyecto llamado `CMSISv2p00_LPC17xx`, borrarlo. Dos proyectos
   no pueden tener el mismo nombre.
2. **Quickstart → Import project(s) from file system... → Project directory (unpacked)**, elegir
   la carpeta `lib/CMSISv2p00_LPC17xx` del repo. **Destildar la opción de copiar al workspace**,
   así se usan los archivos del repo.
3. Repetir con la carpeta `firmware/` del repo: aparece el proyecto `TP_LVDT`.
4. Compilar primero `CMSISv2p00_LPC17xx` y después `TP_LVDT`.
5. **Debug** → elegir el LPC-Link → **Resume** (F8).

Las carpetas `Debug/` y `Release/`, la configuración de depuración (`*.launch`) y `.settings/`
son de cada PC y no se suben (ver `.gitignore`).

## Modos

En `inc/board_config.h`:

- `MODO_SIMULADOR 1` (por ahora): el DAC saca una rampa lenta, de ~0,5 V a ~2,7 V y de vuelta
  en 20 s, que imita la salida del detector con el núcleo moviéndose. Con un cable de J6-18
  (DAC) a J6-15 (ADC) se prueba toda la cadena sin bobinas.
- `MODO_SIMULADOR 0`: funcionamiento normal. El DAC saca el seno de 1 kHz para el amplificador
  del primario y el ADC lee el detector.

## Qué mirar al correrlo

Pausar antes de leer y agregar en **Expressions** `diag`, `ultima_medicion` y `posicion`.

| Expresión | Modo simulador (con el cable) | Modo normal con el cable* |
|---|---|---|
| `diag.arranque` | SUCCESS | SUCCESS |
| `diag.calibracion` | CAL_TABLA_OK | CAL_TABLA_OK |
| `diag.ciclos_por_segundo` | 0 (no hay seno) | 1000 |
| `diag.bloques_por_segundo` | ~125 | ~125 |
| `diag.bloques_perdidos`, `muestras_invalidas`, `muestras_overrun` | 0 | 0 |
| `ultima_medicion.adc_promedio` | `diag.simulador_nivel_esperado` + ~30 | ~2076 |
| `posicion.posicion_mm` | entre 0 y 50, va y vuelve | ~24 (fijo) |
| `posicion.estado` | CAL_OK, o CAL_FUERA_DE_RANGO en los extremos de la rampa | CAL_OK |

\* En modo normal, con el cable el ADC promedia el seno del DAC y la tabla de ejemplo lo
convierte a una posición que no significa nada físicamente. Sirve solo para verificar.

El LED rojo parpadea cada 0,5 s si todo arrancó bien, y cada 0,1 s si falló la configuración
de algún DMA (`diag.arranque` en `ERROR`).

## Calibración con el sensor real

1. Con `MODO_SIMULADOR 0`, la etapa analógica conectada y el control automático sin usar,
   mover el núcleo con la regla de a pasos y anotar en cada posición
   `ultima_medicion.adc_promedio`. Hacer ida y vuelta para ver si se repite.
2. Elegir un tramo donde el nivel siempre suba o siempre baje.
3. Cargar esos puntos en `src/calibration_table.c` y poner `calibracion_tabla_es_ejemplo` en
   `false`.
4. Si `diag.calibracion` no da `CAL_TABLA_OK`, el valor dice qué está mal (puntos
   desordenados, curva que da la vuelta, poca sensibilidad).

## Pruebas en la PC

El módulo de calibración no usa periféricos, así que se prueba en la PC con gcc (por ejemplo
en WSL):

```bash
cd firmware/tests && make
```

Tiene que terminar con `N pruebas, 0 fallas`. La carpeta `tests/` no se compila en MCUXpresso.

## Módulos

| Archivo | Responsabilidad |
|---|---|
| `inc/board_config.h`, `src/board_config.c` | Pines, frecuencias, modo, canales DMA y dueños; inicialización común (reloj, LED, `GPDMA_Init` una sola vez). |
| `src/irq_dispatch.c` | Único `SysTick_Handler` y único `DMA_IRQHandler`; avisan al módulo dueño. |
| `timebase` | Reloj de milisegundos con SysTick y aviso de cada segundo. |
| `excitation` | Seno de 1 kHz por el DAC (P0.26) con DMA en lista circular. |
| `simulator` | Rampa lenta por el DAC que reemplaza al seno en modo simulador. |
| `sampling` | Timer0 dispara el ADC (P0.23) a ~8 kHz; DMA en bloques ping-pong de 64 muestras. |
| `measurement` | Promedio, extremos, tensión y validez de cada bloque. |
| `calibration` | Valida la tabla e interpola el nivel a mm; sin tabla o fuera de rango no inventa posiciones. |
| `src/calibration_table.c` | Tabla que se carga al arrancar (por ahora de ejemplo). |
| `src/main.c` | Orden de arranque y bucle principal; publica `diag`, `ultima_medicion` y `posicion`. |

## Pines

| Pin | J6 | Uso |
|---|---|---|
| P0.26 | 18 | AOUT, salida del DAC hacia el amplificador del primario |
| P0.23 | 15 | AD0.0, entrada desde el detector de envolvente |
| P0.22 | 24 | LED rojo de la placa (se prende con 1 en la rev B/C) |
| P2.0 | 42 | PWM1.1 para el servo (reservado, parte B) |
| P0.2 / P0.3 | 21 / 22 | TXD0 / RXD0 para el adaptador USB-serie (reservado, parte B) |

## Detalles a tener en cuenta

- `GPDMA_Init()` se llama una sola vez, en `board_init()`. Si se vuelve a llamar, resetea todos
  los canales y se corta lo que esté andando.
- Todo lo que lee o escribe el DMA (tablas, buffers y LLI) va en la AHB SRAM con
  `__BSS(RAM2)`: el DMA no accede a la RAM local.
- El contador del DAC dura DACCNTVAL + 1 ciclos, igual que un timer con reset en el match
  (MR + 1). Por eso se carga el valor calculado menos 1.
- En el DMA hay que fijar ancho y ráfaga a mano: en `AUTO`, el driver usa BYTE para el DAC y
  ráfaga de 4 para el ADC, y las dos cosas están mal para este uso.
- El debugger no siempre resetea los periféricos al recargar el programa. Por eso el timer se
  frena antes de configurar el ADC y el simulador apaga explícitamente el DMA del DAC.
- `VREF_MV` en `board_config.h` vale 3300 por defecto. En la rev B/C conviene medir VREF con el
  tester (J6-28 contra J6-1) y corregirlo, porque los 3,3 V pasan por un diodo antes de llegar
  al micro.

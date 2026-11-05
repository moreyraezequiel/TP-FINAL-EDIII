# Firmware

Proyecto de MCUXpresso `TP_LVDT` para la LPCXpresso LPC1769. Sigue la división en módulos y el
reparto de recursos de [ARCHITECTURE.md](../ARCHITECTURE.md).

## Estado

| Parte | Módulo | Estado |
|---|---|---|
| Excitación del primario (DAC + DMA) | `excitation` | Funciona en la placa (issue #19) |
| Muestreo del detector (ADC + Timer0 + DMA) | `sampling` | Funciona en la placa (issue #22) |
| Promedio y calidad de cada bloque | `measurement` | Funciona en la placa; falta convertir a posición |
| Calibración, servo, control y serie | - | Pendiente |

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

## Qué mirar al correrlo

Sin la etapa analógica todavía, se puede probar uniendo J6-18 (salida del DAC) con J6-15
(entrada del ADC). Pausar antes de leer y agregar en **Expressions**:

| Expresión | Esperado con el cable |
|---|---|
| `diag.ciclos_por_segundo` | 1000 (cada tanto 1001) |
| `diag.bloques_por_segundo` | 125 (cada tanto 126) |
| `diag.bloques_perdidos`, `diag.muestras_invalidas`, `diag.muestras_overrun` | 0 |
| `ultima_medicion.adc_promedio` | ~2076 (centro del seno) |
| `ultima_medicion.valida` | true |
| `ultimo_bloque` | 8 valores que se repiten 8 veces |

El LED rojo parpadea cada 0,5 s si todo arrancó bien, y cada 0,1 s si falló la configuración
de algún DMA (`diag.arranque` en `ERROR`).

## Módulos

| Archivo | Responsabilidad |
|---|---|
| `inc/board_config.h`, `src/board_config.c` | Pines, frecuencias, canales DMA y dueños; inicialización común (reloj, LED, `GPDMA_Init` una sola vez). |
| `src/irq_dispatch.c` | Único `SysTick_Handler` y único `DMA_IRQHandler`; avisan al módulo dueño. |
| `timebase` | Reloj de milisegundos con SysTick y aviso de cada segundo. |
| `excitation` | Seno de 1 kHz por el DAC (P0.26) con DMA en lista circular. |
| `sampling` | Timer0 dispara el ADC (P0.23) a ~8 kHz; DMA en bloques ping-pong de 64 muestras. |
| `measurement` | Promedio, extremos, tensión y validez de cada bloque. |
| `src/main.c` | Orden de arranque y bucle principal; publica `diag` y `ultima_medicion`. |

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
- `VREF_MV` en `board_config.h` vale 3300 por defecto. En la rev B/C conviene medir VREF con el
  tester y corregirlo, porque los 3,3 V pasan por un diodo antes de llegar al micro.

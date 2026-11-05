/*
 * board_config.h - Configuración de placa y reparto de recursos.
 *
 * Placa: LPCXpresso LPC1769 rev C (eléctricamente igual a la rev B).
 * Este es el único lugar donde se definen pines, frecuencias y qué módulo es dueño de cada
 * periférico, para que ningún módulo configure algo de otro (ARCHITECTURE.md, sección 6).
 *
 * Pines usados (conector J6):
 *   P0.26 (J6-18)        AOUT   excitation: salida del DAC hacia el amplificador del primario
 *   P0.23 (J6-15)        AD0.0  sampling: entrada del ADC desde el detector de envolvente
 *   P0.22 (J6-24)        GPIO   LED rojo de la placa (se prende con 1 en la rev B/C)
 * Reservados para la parte B (todavía sin implementar):
 *   P2.0  (J6-42)        PWM1.1 servo
 *   P0.2 / P0.3 (J6-21/22) TXD0 / RXD0  serial (adaptador USB-TTL de 3,3 V)
 *
 * Recursos con dueño único:
 *   DAC + su contador, canal DMA 0  -> excitation
 *   Timer0 (MAT0.1), ADC, canal DMA 1 -> sampling
 *   SysTick                          -> timebase
 *   DMA_IRQHandler, SysTick_Handler  -> irq_dispatch.c (un solo handler por interrupción)
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "LPC17xx.h"
#include "lpc17xx_gpdma.h"
#include "lpc17xx_gpio.h"

/* LED de la placa */
#define LED_PUERTO  PORT_0
#define LED_MASCARA (1u << 22)

/* Canales de DMA (el 0 tiene más prioridad que el 1) */
#define DMA_CANAL_EXCITACION GPDMA_CH_0 /* tabla -> DAC */
#define DMA_CANAL_MUESTREO   GPDMA_CH_1 /* ADC -> buffers */

/* Excitación del primario */
#define EXCITACION_HZ       1000u /* frecuencia del seno */
#define EXCITACION_MUESTRAS 32u   /* muestras por ciclo -> el DAC se actualiza a 32 kHz */

/* Muestreo del detector */
#define MUESTREO_HZ     8000u /* pedido; con PCLK de 25 MHz da 8002,6 muestras/s */
#define MUESTREO_BLOQUE 64u   /* muestras por bloque: un bloque cada 8 ms */

/* Referencia del ADC/DAC. En la rev B/C los 3,3 V pasan por un diodo antes de llegar al
 * micro: medir VREF con el tester y corregir este valor. Solo se usa para pasar a mV. */
#define VREF_MV 3300u

/* Reloj, LED, GPDMA (se inicializa UNA sola vez) e interrupción del DMA. */
void board_init(void);
void board_led_toggle(void);

#endif /* BOARD_CONFIG_H */

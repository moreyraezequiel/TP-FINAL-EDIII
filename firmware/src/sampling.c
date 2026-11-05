/*
 * sampling.c - ADC disparado por Timer0 + DMA en bloques ping-pong.
 *
 * Timer0: MAT0.1 cambia de estado (toggle) a ~16 kHz. Cada flanco de subida arranca una
 * conversión del ADC, así que se muestrea a ~8 kHz. MAT0.1 no necesita salir a un pin
 * (manual UM10360, 29.5.1).
 *
 * ADC -> DMA: el pedido de DMA del ADC es la misma señal que su interrupción (UM10360,
 * 29.6.4). Se habilita solo la interrupción global y el DMA lee ADGDR: esa lectura borra DONE,
 * así que hay exactamente una transferencia por conversión. La IRQ del ADC queda apagada en
 * el NVIC y el DMA usa ráfagas de 1 (el ADC no admite otra cosa con un solo canal).
 */

#include "sampling.h"

#include <cr_section_macros.h>
#include <stddef.h>

#include "board_config.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_clkpwr.h"
#include "lpc17xx_timer.h"

/* Lo que escribe el DMA va en la AHB SRAM (RAM2, 0x2007C000). */
__BSS(RAM2) static uint32_t buf_adc[2][MUESTREO_BLOQUE];
__BSS(RAM2) static GPDMA_LLI_T lli_adc[2];

static volatile uint32_t bloques;       /* bloques completos desde el arranque */
static volatile uint32_t bloque_listo;  /* buffer (0 o 1) que terminó de llenarse */
static volatile uint32_t hay_bloque;    /* 1 = hay un bloque sin retirar */
static volatile uint32_t perdidos;
static volatile uint32_t bloques_por_segundo;

static void config_timer(void) {
    /* TIM_InitTimer también lo frena y lo pone en cero, por si quedó andando de una corrida
     * anterior: así no hay conversiones mientras se configura el resto. */
    const TIM_TIMERCFG_T tim = {.prescaleOpt = TIM_TICK, .prescaleValue = 1}; /* 25 MHz */
    TIM_InitTimer(LPC_TIM0, &tim);

    /* Con reset en el match, cada medio período dura MR + 1 cuentas.
     * 25 MHz / (2 x 8 kHz) = 1562 (no da exacto: 8002,6 muestras/s). */
    const uint32_t medio_periodo = CLKPWR_GetPCLK(CLKPWR_PCLKSEL_TIMER0) / (2u * MUESTREO_HZ);
    const TIM_MATCHCFG_T match   = {
          .channel    = TIM_MATCH_1,
          .intEn      = DISABLE,
          .stopEn     = DISABLE,
          .resetEn    = ENABLE,
          .extOpt     = TIM_TOGGLE,
          .matchValue = medio_periodo - 1u,
    };
    TIM_ConfigMatch(LPC_TIM0, &match);
}

static void config_adc(void) {
    ADC_Init(200000);             /* reloj del ADC ~12,5 MHz: cada conversión tarda ~5 us */
    ADC_PinConfig(ADC_CHANNEL_0); /* P0.23 = AD0.0 */
    ADC_ChannelEnable(ADC_CHANNEL_0);
    ADC_EdgeStartConfig(ADC_START_ON_RISING);
    ADC_IntDisable(ADC_INT_CH0);
    ADC_IntEnable(ADC_INT_GLOBAL);
}

static Status config_dma(void) {
    for (uint32_t k = 0; k < 2u; k++) {
        lli_adc[k].srcAddr = (uint32_t)&LPC_ADC->ADGDR;
        lli_adc[k].dstAddr = (uint32_t)buf_adc[k];
        lli_adc[k].nextLLI = (uint32_t)&lli_adc[1u - k]; /* 0 -> 1 -> 0 -> ... */
        lli_adc[k].control = GPDMA_DMACCxControl_TransferSize(MUESTREO_BLOQUE) |
                             GPDMA_DMACCxControl_SWidth(GPDMA_WORD) |
                             GPDMA_DMACCxControl_DWidth(GPDMA_WORD) |
                             GPDMA_DMACCxControl_DI | GPDMA_DMACCxControl_I;
    }

    GPDMA_Channel_CFG_T cfg = {0};
    cfg.channelNum    = DMA_CANAL_MUESTREO;
    cfg.type          = GPDMA_P2M;
    cfg.transferSize  = MUESTREO_BLOQUE;
    cfg.srcConn       = GPDMA_ADC; /* el driver pone ADGDR como origen */
    cfg.dstMemAddr    = (uint32_t)buf_adc[0];
    cfg.src.width     = GPDMA_WORD;
    cfg.dst.width     = GPDMA_WORD;
    cfg.src.burst     = GPDMA_BSIZE_1; /* en AUTO el driver pondría 4 */
    cfg.dst.burst     = GPDMA_BSIZE_1;
    cfg.src.increment = DISABLE;
    cfg.dst.increment = ENABLE;
    cfg.intTC         = ENABLE;
    cfg.intErr        = DISABLE;
    cfg.linkedList    = (uint32_t)&lli_adc[1]; /* después de buf_adc[0] sigue buf_adc[1] */
    if (GPDMA_SetupChannel(&cfg) != SUCCESS) {
        return ERROR;
    }
    GPDMA_ChannelStart(DMA_CANAL_MUESTREO);
    return SUCCESS;
}

Status sampling_init(void) {
    config_timer(); /* primero: deja el timer frenado */
    config_adc();
    if (config_dma() != SUCCESS) {
        return ERROR;
    }
    ADC_StartCmd(ADC_START_ON_MAT01); /* convierte en cada flanco de MAT0.1 (cuando arranque) */
    return SUCCESS;
}

void sampling_start(void) {
    TIM_Enable(LPC_TIM0);
}

const volatile uint32_t* sampling_take_block(void) {
    if (!hay_bloque) {
        return NULL;
    }
    __disable_irq(); /* tomar el índice y bajar la bandera sin que se meta el DMA */
    const uint32_t idx = bloque_listo;
    hay_bloque         = 0;
    __enable_irq();
    return buf_adc[idx];
}

uint32_t sampling_blocks_per_second(void) {
    return bloques_por_segundo;
}

uint32_t sampling_lost_blocks(void) {
    return perdidos;
}

void sampling_on_dma_tc(void) {
    if (hay_bloque) {
        perdidos++;
    }
    bloque_listo = bloques & 1u; /* el 1.er bloque llena buf_adc[0], el 2.o buf_adc[1]... */
    hay_bloque   = 1;
    bloques++;
}

void sampling_on_second(void) {
    static uint32_t bloques_antes = 0;

    bloques_por_segundo = bloques - bloques_antes;
    bloques_antes       = bloques;
}

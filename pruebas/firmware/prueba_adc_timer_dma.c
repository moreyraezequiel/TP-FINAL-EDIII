/*
 * Prueba 2 del TP: ADC disparado por Timer0 a 8 kHz, con DMA en bloques de 64 (ping-pong).
 *
 * Placa: LPCXpresso LPC1769 rev B/C. Librería: CMSISv2p00_LPC17xx (drivers de la cátedra).
 * Cable: J6-18 (P0.26, salida del DAC) con J6-15 (P0.23, entrada del ADC).
 *
 *  - DAC + DMA canal 0: el mismo seno de 1 kHz de la prueba 1, como señal de entrada.
 *  - Timer0: MAT0.1 cambia de estado (toggle) a ~16 kHz. Cada flanco de subida arranca una
 *    conversión del ADC, así que se muestrea a ~8 kHz sin usar la CPU.
 *  - DMA canal 1: copia cada resultado del ADC a dos buffers de 64 muestras que se alternan.
 *    Mientras se llena uno, el programa procesa el otro. Cada bloque dura 8 ms.
 *
 * Qué mirar en el debugger (pausá antes de leer, Expressions):
 *  - bloques_por_segundo: ~125 (8002,6 / 64; cada tanto 126).
 *  - ciclos_por_segundo:  ~1000 (frecuencia del seno).
 *  - bloque_promedio:     ~2078 (el centro del seno: 64 muestras son 8 ciclos completos).
 *  - ultimo_bloque[64]:   el mismo grupo de 8 valores repetido 8 veces (8 muestras por ciclo).
 *  - muestras_invalidas, muestras_overrun, bloques_perdidos: tienen que quedar en 0.
 */

#include "LPC17xx.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_clkpwr.h"
#include "lpc17xx_dac.h"
#include "lpc17xx_gpdma.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_systick.h"
#include "lpc17xx_timer.h"
#include <cr_section_macros.h>

#define SENO_N    32u                /* muestras por ciclo del seno */
#define SENO_HZ   1000u
#define DAC_FS_HZ (SENO_N * SENO_HZ) /* 32 kHz */

#define ADC_FS_HZ 8000u /* muestras por segundo del ADC */
#define BLOQUE_N  64u   /* muestras por bloque: 8 ms */

#define CANAL_DAC GPDMA_CH_0
#define CANAL_ADC GPDMA_CH_1

#define LED_ROJO (1u << 22) /* P0.22, se prende con 1 en la rev B/C */

/* Códigos del DAC (10 bits): 512 + round(219 * sen(2*pi*n/32)) */
static const uint16_t seno[SENO_N] = {
    512, 555, 596, 634, 667, 694, 714, 727, 731, 727, 714, 694, 667, 634, 596, 555,
    512, 469, 428, 390, 357, 330, 310, 297, 293, 297, 310, 330, 357, 390, 428, 469,
};

/* Todo lo que toca el DMA va en la AHB SRAM (RAM2, 0x2007C000). */
__BSS(RAM2) static uint32_t tabla_dac[SENO_N];
__BSS(RAM2) static GPDMA_LLI_T lli_dac;
__BSS(RAM2) static uint32_t buf_adc[2][BLOQUE_N];
__BSS(RAM2) static GPDMA_LLI_T lli_adc[2];

/* Resultados para mirar en el debugger. */
volatile uint32_t ciclos_por_segundo;  /* frecuencia del seno, en Hz */
volatile uint32_t bloques_por_segundo; /* bloques del ADC por segundo */
volatile uint16_t bloque_promedio, bloque_min, bloque_max;
volatile uint16_t ultimo_bloque[BLOQUE_N];
volatile uint32_t bloques_procesados;
volatile uint32_t bloques_perdidos;   /* llegó un bloque antes de procesar el anterior */
volatile uint32_t muestras_invalidas; /* el DMA leyó un resultado sin DONE */
volatile uint32_t muestras_overrun;   /* el ADC pisó un resultado que nadie había leído */
volatile Status estado_dma;

static volatile uint32_t ms;          /* milisegundos desde el arranque */
static volatile uint32_t ciclos_dac;  /* pasadas completas de la tabla del DAC */
static volatile uint32_t bloques_adc; /* bloques completos del ADC */
static volatile uint32_t bloque_listo; /* buffer (0 o 1) que terminó de llenarse */
static volatile uint32_t hay_bloque;   /* 1 = hay un bloque esperando ser procesado */

void SysTick_Handler(void) {
    static uint32_t ciclos_antes  = 0;
    static uint32_t bloques_antes = 0;

    ms++;
    if (ms % 1000u == 0) { /* cada 1 s exacto */
        ciclos_por_segundo  = ciclos_dac - ciclos_antes;
        ciclos_antes        = ciclos_dac;
        bloques_por_segundo = bloques_adc - bloques_antes;
        bloques_antes       = bloques_adc;
    }
}

/* Un solo handler para todo el DMA: se fija qué canal terminó. */
void DMA_IRQHandler(void) {
    if (GPDMA_IntGetStatus(GPDMA_INTTC, CANAL_DAC) == SET) {
        GPDMA_ClearIntPending(GPDMA_CLR_INTTC, CANAL_DAC);
        ciclos_dac++;
    }
    if (GPDMA_IntGetStatus(GPDMA_INTTC, CANAL_ADC) == SET) {
        GPDMA_ClearIntPending(GPDMA_CLR_INTTC, CANAL_ADC);
        if (hay_bloque) {
            bloques_perdidos++;
        }
        bloque_listo = bloques_adc & 1u; /* el 1.er bloque llena buf_adc[0], el 2.o buf_adc[1]... */
        hay_bloque   = 1;
        bloques_adc++;
    }
}

static Status config_seno_dac_dma(void) {
    for (uint32_t n = 0; n < SENO_N; n++) {
        tabla_dac[n] = (uint32_t)seno[n] << 6; /* DACR: valor en los bits 15:6 */
    }

    DAC_Init(); /* P0.26 como AOUT, PCLK del DAC = 25 MHz */
    /* El contador cuenta DACCNTVAL + 1 ciclos por muestra: 25 MHz / 32 kHz - 1 = 780. */
    DAC_SetDMATimeOut((uint16_t)(CLKPWR_GetPCLK(CLKPWR_PCLKSEL_DAC) / DAC_FS_HZ - 1u));

    lli_dac.srcAddr = (uint32_t)tabla_dac;
    lli_dac.dstAddr = (uint32_t)&LPC_DAC->DACR;
    lli_dac.nextLLI = (uint32_t)&lli_dac; /* se repite sola */
    lli_dac.control = GPDMA_DMACCxControl_TransferSize(SENO_N) |
                      GPDMA_DMACCxControl_SWidth(GPDMA_WORD) |
                      GPDMA_DMACCxControl_DWidth(GPDMA_WORD) |
                      GPDMA_DMACCxControl_SI | GPDMA_DMACCxControl_I;

    GPDMA_Channel_CFG_T cfg = {0};
    cfg.channelNum    = CANAL_DAC;
    cfg.type          = GPDMA_M2P;
    cfg.transferSize  = SENO_N;
    cfg.srcMemAddr    = (uint32_t)tabla_dac;
    cfg.dstConn       = GPDMA_DAC;
    cfg.src.width     = GPDMA_WORD; /* explícito: en AUTO el driver usa BYTE para el DAC */
    cfg.dst.width     = GPDMA_WORD;
    cfg.src.burst     = GPDMA_BSIZE_1;
    cfg.dst.burst     = GPDMA_BSIZE_1;
    cfg.src.increment = ENABLE;
    cfg.dst.increment = DISABLE;
    cfg.intTC         = ENABLE;
    cfg.intErr        = DISABLE;
    cfg.linkedList    = (uint32_t)&lli_dac;
    if (GPDMA_SetupChannel(&cfg) != SUCCESS) {
        return ERROR;
    }
    GPDMA_ChannelStart(CANAL_DAC);

    DAC_CONVERTER_CFG_T dac = {
        .doubleBuffer = ENABLE,
        .dmaCounter   = ENABLE,
        .dmaRequest   = ENABLE,
    };
    DAC_ConfigDAConverterControl(&dac);
    return SUCCESS;
}

static Status config_adc_dma(void) {
    ADC_Init(200000);             /* reloj del ADC ~12,5 MHz: cada conversión tarda ~5 us */
    ADC_PinConfig(ADC_CHANNEL_0); /* P0.23 = AD0.0 */
    ADC_ChannelEnable(ADC_CHANNEL_0);
    ADC_EdgeStartConfig(ADC_START_ON_RISING);

    /* El pedido de DMA del ADC es la misma señal que su interrupción (manual, 29.6.4).
     * Se habilita solo la global: el DMA lee ADGDR y esa lectura borra DONE, así que pide
     * una sola transferencia por conversión. La IRQ del ADC queda apagada en el NVIC. */
    ADC_IntDisable(ADC_INT_CH0);
    ADC_IntEnable(ADC_INT_GLOBAL);

    for (uint32_t k = 0; k < 2u; k++) {
        lli_adc[k].srcAddr = (uint32_t)&LPC_ADC->ADGDR;
        lli_adc[k].dstAddr = (uint32_t)buf_adc[k];
        lli_adc[k].nextLLI = (uint32_t)&lli_adc[1u - k]; /* 0 -> 1 -> 0 -> ... */
        lli_adc[k].control = GPDMA_DMACCxControl_TransferSize(BLOQUE_N) |
                             GPDMA_DMACCxControl_SWidth(GPDMA_WORD) |
                             GPDMA_DMACCxControl_DWidth(GPDMA_WORD) |
                             GPDMA_DMACCxControl_DI | GPDMA_DMACCxControl_I;
    }

    GPDMA_Channel_CFG_T cfg = {0};
    cfg.channelNum    = CANAL_ADC;
    cfg.type          = GPDMA_P2M;
    cfg.transferSize  = BLOQUE_N;
    cfg.srcConn       = GPDMA_ADC; /* el driver pone ADGDR como origen */
    cfg.dstMemAddr    = (uint32_t)buf_adc[0];
    cfg.src.width     = GPDMA_WORD;
    cfg.dst.width     = GPDMA_WORD;
    cfg.src.burst     = GPDMA_BSIZE_1; /* el ADC solo admite ráfaga de 1 (en AUTO serían 4) */
    cfg.dst.burst     = GPDMA_BSIZE_1;
    cfg.src.increment = DISABLE;
    cfg.dst.increment = ENABLE;
    cfg.intTC         = ENABLE;
    cfg.intErr        = DISABLE;
    cfg.linkedList    = (uint32_t)&lli_adc[1]; /* después de buf_adc[0] sigue buf_adc[1] */
    if (GPDMA_SetupChannel(&cfg) != SUCCESS) {
        return ERROR;
    }
    GPDMA_ChannelStart(CANAL_ADC);

    ADC_StartCmd(ADC_START_ON_MAT01); /* desde ahora convierte en cada flanco de MAT0.1 */
    return SUCCESS;
}

static void config_timer_adc(void) {
    const TIM_TIMERCFG_T tim = {.prescaleOpt = TIM_TICK, .prescaleValue = 1}; /* 25 MHz */
    TIM_InitTimer(LPC_TIM0, &tim);

    /* MAT0.1 cambia cada medio período; con reset en el match, cada medio período dura
     * MR + 1 cuentas. 25 MHz / (2 x 8 kHz) = 1562 (no da exacto: 8002,6 muestras/s). */
    const uint32_t medio_periodo = CLKPWR_GetPCLK(CLKPWR_PCLKSEL_TIMER0) / (2u * ADC_FS_HZ);
    const TIM_MATCHCFG_T match   = {
          .channel    = TIM_MATCH_1,
          .intEn      = DISABLE,
          .stopEn     = DISABLE,
          .resetEn    = ENABLE,
          .extOpt     = TIM_TOGGLE,
          .matchValue = medio_periodo - 1u,
    };
    TIM_ConfigMatch(LPC_TIM0, &match);
    /* No se arranca acá: TIM_Enable va al final, cuando el ADC y el DMA ya están listos. */
}

static void procesar_bloque(const volatile uint32_t* b) {
    uint32_t suma = 0;
    uint16_t mn   = 4095;
    uint16_t mx   = 0;

    for (uint32_t i = 0; i < BLOQUE_N; i++) {
        const uint32_t w = b[i];
        if ((w & ADC_DR_DONE_FLAG) == 0) {
            muestras_invalidas++;
        }
        if (w & ADC_DR_OVERRUN_FLAG) {
            muestras_overrun++;
        }
        const uint16_t v = (uint16_t)ADC_GDR_RESULT(w);
        ultimo_bloque[i] = v;
        suma += v;
        if (v < mn) {
            mn = v;
        }
        if (v > mx) {
            mx = v;
        }
    }

    bloque_promedio = (uint16_t)(suma / BLOQUE_N);
    bloque_min      = mn;
    bloque_max      = mx;
    bloques_procesados++;
}

int main(void) {
    SystemCoreClockUpdate();

    GPIO_SetDir(PORT_0, LED_ROJO, GPIO_OUTPUT);

    SYSTICK_InternalInit(1); /* interrupción cada 1 ms */
    SYSTICK_IntCmd(ENABLE);
    SYSTICK_Cmd(ENABLE);

    GPDMA_Init(); /* UNA sola vez: si se llama de nuevo, resetea todos los canales */
    NVIC_EnableIRQ(DMA_IRQn);

    /* Orden de arranque: primero el timer (TIM_InitTimer lo frena y lo pone en cero, por si
     * quedó andando de una corrida anterior), después DAC y ADC con su DMA, y recién al final
     * se larga el timer. Así no hay conversiones mientras se configura nada. */
    config_timer_adc();
    estado_dma = config_seno_dac_dma();
    if (estado_dma == SUCCESS) {
        estado_dma = config_adc_dma();
    }
    TIM_Enable(LPC_TIM0);

    const uint32_t periodo_ms = (estado_dma == SUCCESS) ? 500u : 100u;
    uint32_t proximo          = 0;

    while (1) {
        if (hay_bloque) {
            __disable_irq(); /* tomar el índice y bajar la bandera sin que se meta el DMA */
            const uint32_t idx = bloque_listo;
            hay_bloque         = 0;
            __enable_irq();
            procesar_bloque(buf_adc[idx]);
        }

        if ((int32_t)(ms - proximo) >= 0) {
            proximo += periodo_ms;
            GPIO_TogglePins(PORT_0, LED_ROJO);
        }
    }
}

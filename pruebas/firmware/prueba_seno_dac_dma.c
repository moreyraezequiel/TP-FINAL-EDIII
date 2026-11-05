/*
 * Prueba 1 del TP: seno de 1 kHz con DAC + DMA, verificado con el ADC.
 *
 * Placa: LPCXpresso LPC1769 rev B/C. Librería: CMSISv2p00_LPC17xx (drivers de la cátedra).
 *
 *  - DAC (P0.26, J6-18): 32 muestras por ciclo a 32.000 muestras/s = 1 kHz.
 *    Amplitud 219 cuentas (~0,5 V RMS), centrada en 512 (~VREF/2).
 *  - DMA canal 0: copia la tabla al DAC sin usar la CPU. La LLI apunta a sí misma,
 *    así que la tabla se repite para siempre.
 *  - ADC (P0.23, J6-15): con un cable de J6-18 a J6-15 mide la salida del DAC.
 *    Cada 0,5 s toma 256 muestras y guarda mínimo, máximo, centro y pico a pico.
 *    Esperado: min ~1172, max ~2924, centro ~2048, pp ~1752 (ADC = 4 x código del DAC).
 *  - LED P0.22: parpadea cada 0,5 s si todo arrancó bien, y rápido (0,1 s) si falló el DMA.
 *  - Frecuencia: el DMA avisa (interrupción) cada vez que termina la tabla, o sea en cada
 *    ciclo del seno. Cada 1 s exacto se guarda la cuenta en ciclos_por_segundo.
 *    Esperado: 1000 (la cuenta da 1000,3 Hz, así que cada ~3 s puede aparecer 1001).
 */

#include "LPC17xx.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_clkpwr.h"
#include "lpc17xx_dac.h"
#include "lpc17xx_gpdma.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_systick.h"
#include <cr_section_macros.h>

#define SENO_N    32u                    /* muestras por ciclo */
#define SENO_HZ   1000u
#define DAC_FS_HZ (SENO_N * SENO_HZ)     /* 32 kHz */
#define LED_ROJO  (1u << 22)             /* P0.22, se prende con 1 en la rev B/C */
#define ADC_N     256u                   /* muestras por medición (~1,5 ciclos) */

/* Códigos del DAC (10 bits): 512 + round(219 * sen(2*pi*n/32)) */
static const uint16_t seno[SENO_N] = {
    512, 555, 596, 634, 667, 694, 714, 727, 731, 727, 714, 694, 667, 634, 596, 555,
    512, 469, 428, 390, 357, 330, 310, 297, 293, 297, 310, 330, 357, 390, 428, 469,
};

/* Tabla y LLI en la AHB SRAM (RAM2, 0x2007C000): el DMA no llega a la RAM local. */
__BSS(RAM2) static uint32_t tabla_dac[SENO_N];
__BSS(RAM2) static GPDMA_LLI_T lli_dac;

/* Resultados para mirar en el debugger (pestaña Expressions). */
volatile uint16_t adc_min, adc_max, adc_centro, adc_pp;
volatile uint32_t mediciones;
volatile Status estado_dma;
volatile uint32_t ciclos_por_segundo; /* frecuencia medida del seno, en Hz */

static volatile uint32_t ms;         /* milisegundos desde el arranque */
static volatile uint32_t ciclos_dac; /* pasadas completas de la tabla */

void SysTick_Handler(void) {
    static uint32_t ciclos_antes = 0;

    ms++;
    if (ms % 1000u == 0) { /* cada 1 s exacto */
        ciclos_por_segundo = ciclos_dac - ciclos_antes;
        ciclos_antes       = ciclos_dac;
    }
}

void DMA_IRQHandler(void) {
    if (GPDMA_IntGetStatus(GPDMA_INTTC, GPDMA_CH_0) == SET) {
        GPDMA_ClearIntPending(GPDMA_CLR_INTTC, GPDMA_CH_0);
        ciclos_dac++; /* terminó una pasada de la tabla = un ciclo del seno */
    }
}

static Status config_seno_dac_dma(void) {
    /* DACR guarda el valor en los bits 15:6; BIAS (bit 16) queda en 0. */
    for (uint32_t n = 0; n < SENO_N; n++) {
        tabla_dac[n] = (uint32_t)seno[n] << 6;
    }

    DAC_Init(); /* P0.26 como AOUT y PCLK del DAC = CCLK/4 = 25 MHz */
    /* El contador va de DACCNTVAL hasta 0 inclusive: cada muestra dura DACCNTVAL + 1 ciclos.
     * Por eso se carga 25 MHz / 32 kHz - 1 = 780 (con 781 daba 999 Hz, medido). */
    DAC_SetDMATimeOut((uint16_t)(CLKPWR_GetPCLK(CLKPWR_PCLKSEL_DAC) / DAC_FS_HZ - 1u));

    lli_dac.srcAddr = (uint32_t)tabla_dac;
    lli_dac.dstAddr = (uint32_t)&LPC_DAC->DACR;
    lli_dac.nextLLI = (uint32_t)&lli_dac; /* apunta a sí misma: se repite sola */
    lli_dac.control = GPDMA_DMACCxControl_TransferSize(SENO_N) |
                      GPDMA_DMACCxControl_SWidth(GPDMA_WORD) |
                      GPDMA_DMACCxControl_DWidth(GPDMA_WORD) |
                      GPDMA_DMACCxControl_SI |   /* avanza el origen */
                      GPDMA_DMACCxControl_I;     /* avisa al terminar cada pasada */

    GPDMA_Init();

    GPDMA_Channel_CFG_T cfg = {0};
    cfg.channelNum    = GPDMA_CH_0;
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
    cfg.intTC         = ENABLE; /* habilita las interrupciones de fin de bloque del canal */
    cfg.intErr        = DISABLE;
    cfg.linkedList    = (uint32_t)&lli_dac;

    if (GPDMA_SetupChannel(&cfg) != SUCCESS) {
        return ERROR;
    }
    NVIC_EnableIRQ(DMA_IRQn);
    GPDMA_ChannelStart(GPDMA_CH_0);

    /* Contador del DAC + pedidos de DMA + doble buffer: una muestra cada 31,25 us. */
    DAC_CONVERTER_CFG_T dac = {
        .doubleBuffer = ENABLE,
        .dmaCounter   = ENABLE,
        .dmaRequest   = ENABLE,
    };
    DAC_ConfigDAConverterControl(&dac);

    return SUCCESS;
}

static void config_adc(void) {
    ADC_Init(200000);             /* reloj del ADC ~12,5 MHz */
    ADC_PinConfig(ADC_CHANNEL_0); /* P0.23 = AD0.0 */
    ADC_ChannelEnable(ADC_CHANNEL_0);
}

static uint16_t adc_leer(void) {
    ADC_StartCmd(ADC_START_NOW);
    while (ADC_ChannelGetStatus(ADC_CHANNEL_0, ADC_DATA_DONE) == RESET) {
    }
    return ADC_ChannelGetData(ADC_CHANNEL_0);
}

static void medir_salida_dac(void) {
    uint16_t mn = 4095;
    uint16_t mx = 0;

    for (uint32_t i = 0; i < ADC_N; i++) {
        const uint16_t v = adc_leer();
        if (v < mn) {
            mn = v;
        }
        if (v > mx) {
            mx = v;
        }
    }

    adc_min    = mn;
    adc_max    = mx;
    adc_centro = (uint16_t)((mn + mx) / 2u);
    adc_pp     = (uint16_t)(mx - mn);
    mediciones++;
}

int main(void) {
    SystemCoreClockUpdate();

    GPIO_SetDir(PORT_0, LED_ROJO, GPIO_OUTPUT);

    SYSTICK_InternalInit(1); /* interrupción cada 1 ms */
    SYSTICK_IntCmd(ENABLE);
    SYSTICK_Cmd(ENABLE);

    estado_dma = config_seno_dac_dma();
    config_adc();

    const uint32_t periodo_ms = (estado_dma == SUCCESS) ? 500u : 100u;
    uint32_t proximo          = 0;

    while (1) {
        if ((int32_t)(ms - proximo) >= 0) {
            proximo += periodo_ms;
            GPIO_TogglePins(PORT_0, LED_ROJO);
            medir_salida_dac();
        }
    }
}

/*
 * excitation.c - Seno de excitación por DAC + DMA.
 *
 * El DMA copia una tabla de EXCITACION_MUESTRAS valores al DAC, una muestra por cada vez que
 * vence el contador del DAC. La LLI apunta a sí misma, así que la tabla se repite sola.
 * Cada pasada completa genera una interrupción, que se usa para medir la frecuencia.
 */

#include "excitation.h"

#include <cr_section_macros.h>

#include "board_config.h"
#include "lpc17xx_clkpwr.h"
#include "lpc17xx_dac.h"

#define DAC_FS_HZ (EXCITACION_MUESTRAS * EXCITACION_HZ) /* 32 kHz */

/* Códigos del DAC (10 bits): 512 + round(219 * sen(2*pi*n/32)) */
static const uint16_t seno[EXCITACION_MUESTRAS] = {
    512, 555, 596, 634, 667, 694, 714, 727, 731, 727, 714, 694, 667, 634, 596, 555,
    512, 469, 428, 390, 357, 330, 310, 297, 293, 297, 310, 330, 357, 390, 428, 469,
};

/* Lo que lee el DMA va en la AHB SRAM (RAM2, 0x2007C000): el DMA no llega a la RAM local. */
__BSS(RAM2) static uint32_t tabla_dac[EXCITACION_MUESTRAS];
__BSS(RAM2) static GPDMA_LLI_T lli_dac;

static volatile uint32_t ciclos;             /* pasadas completas de la tabla */
static volatile uint32_t ciclos_por_segundo;

Status excitation_start(void) {
    for (uint32_t n = 0; n < EXCITACION_MUESTRAS; n++) {
        tabla_dac[n] = (uint32_t)seno[n] << 6; /* DACR: valor en los bits 15:6, BIAS = 0 */
    }

    DAC_Init(); /* P0.26 como AOUT, PCLK del DAC = CCLK/4 = 25 MHz */
    /* El contador va de DACCNTVAL hasta 0 inclusive: cada muestra dura DACCNTVAL + 1 ciclos.
     * 25 MHz / 32 kHz - 1 = 780 -> 1000,3 Hz (con 781 se midieron 999 Hz). */
    DAC_SetDMATimeOut((uint16_t)(CLKPWR_GetPCLK(CLKPWR_PCLKSEL_DAC) / DAC_FS_HZ - 1u));

    lli_dac.srcAddr = (uint32_t)tabla_dac;
    lli_dac.dstAddr = (uint32_t)&LPC_DAC->DACR;
    lli_dac.nextLLI = (uint32_t)&lli_dac; /* se repite sola */
    lli_dac.control = GPDMA_DMACCxControl_TransferSize(EXCITACION_MUESTRAS) |
                      GPDMA_DMACCxControl_SWidth(GPDMA_WORD) |
                      GPDMA_DMACCxControl_DWidth(GPDMA_WORD) |
                      GPDMA_DMACCxControl_SI | /* avanza el origen */
                      GPDMA_DMACCxControl_I;   /* avisa al terminar cada pasada */

    GPDMA_Channel_CFG_T cfg = {0};
    cfg.channelNum    = DMA_CANAL_EXCITACION;
    cfg.type          = GPDMA_M2P;
    cfg.transferSize  = EXCITACION_MUESTRAS;
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
    GPDMA_ChannelStart(DMA_CANAL_EXCITACION);

    /* Contador + pedidos de DMA + doble buffer: una muestra cada 31,25 us, sin jitter. */
    const DAC_CONVERTER_CFG_T dac = {
        .doubleBuffer = ENABLE,
        .dmaCounter   = ENABLE,
        .dmaRequest   = ENABLE,
    };
    DAC_ConfigDAConverterControl(&dac);
    return SUCCESS;
}

uint32_t excitation_cycles_per_second(void) {
    return ciclos_por_segundo;
}

void excitation_on_dma_tc(void) {
    ciclos++;
}

void excitation_on_second(void) {
    static uint32_t ciclos_antes = 0;

    ciclos_por_segundo = ciclos - ciclos_antes;
    ciclos_antes       = ciclos;
}

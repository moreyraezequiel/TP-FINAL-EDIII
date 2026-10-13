# TP final de Electrónica Digital III

## Girasol electrónico: seguidor de luz con LPC1769



### 1. Idea y objetivo

Un servo gira una "cabeza" con dos sensores de luz (LDR) separados por una pared de cartón. Si la luz viene de la izquierda, la pared le hace sombra al LDR derecho: el izquierdo mide más y la LPC1769 gira el servo hacia la izquierda. Cuando los dos miden lo mismo, la cabeza apunta a la luz y se queda quieta. Al mover una linterna, el girasol la sigue.

```text
LDR izq ─┐
         ├─► ADC (burst) ─► DMA ─► buffers ─┐
LDR der ─┘                                  │
                      Timer0 (50 Hz) ──────►├─► control: comparar y dar un paso ─► PWM1 ─► servo
                                            │                                            │
                                            │          el servo gira los LDR ◄───────────┘
                                            ├─► DAC ─► osciloscopio (monitor del error / ángulo / luz)
Botones (EINT / GPIO) ─► modos              └─► UART (+ DMA) ─► página web
```

**Por qué es realimentado:** los sensores van montados sobre lo que se mueve. Al girar, cambia lo que miden, y el micro corrige según esa medición. Para mostrarlo, el modo **manual** deja el servo quieto aunque la luz se vaya, y el modo **automático** la sigue.

**Enfoque del TP:** el peso está en los periféricos de Digital 3. El control es un "paso a paso" de pocas líneas (sección 5.6), sin ganancias que ajustar.

### 2. Periféricos y para qué se usa cada uno

| Periférico | Uso | ¿Natural o forzado? |
| --- | --- | --- |
| **ADC** | Lee los LDR en modo burst: recorre los canales solo, sin que la CPU lo pida. | Natural |
| **GPDMA** | Canal 0: ADC → buffers ping-pong. Canal 1: trama → UART. *(Extra: canal 2, repetición de la señal cruda en el DAC; sección 6.)* | Natural: son cientos de muestras por segundo. |
| **Timer0** | Interrupción cada 20 ms: corre el control al mismo ritmo que el pulso del servo. | Natural |
| **PWM1** | Señal del servo: un pulso de ~1–2 ms cada 20 ms (PWM1.1). | Natural; es el uso clásico. |
| **DAC** | **Monitor para el osciloscopio:** saca como tensión una variable interna (error, ángulo o luz de un LDR) para verla en tiempo real en el osciloscopio. **Autotest al arrancar:** genera tensiones conocidas que el ADC lee por un cable y verifica (sección 5.7). | Herramienta real de depuración y prueba; sin componentes. |
| **UART0** | Telemetría y comandos para la página web. | Natural |
| **EINT1 / EINT2** | Botón de modo (automático / manual / vampiro) y botón de barrido / baile. | Natural |
| **Interrupciones GPIO** | Botones para girar a mano a izquierda y derecha (comparten el vector de EINT3). | Natural |
| **SysTick** | Base de tiempo de 1 ms, antirrebote y ritmo de la telemetría. | Natural |
| **NVIC** | Prioridades (sección 5.4). | Natural |
| **Modo Sleep** | La CPU duerme entre interrupciones; en el modo "dormido" (sección 6) casi no se despierta. | Natural |

### 3. Hardware

#### 3.1 Lista de compras

| Componente | Cantidad | Para qué |
| --- | --- | --- |
| Placa LPCXpresso LPC1769 | 1 | Ya la tienen. |
| Adaptador USB-UART **CP2102** (TX/RX de 3,3 V) | 1 | Comunicación con la PC. Omitirlo si la placa trae un puerto serie virtual utilizable. |
| Servo **SG90** ("micro servo 9g") | 1 | Gira la cabeza. |
| LDR de 5 mm (GL5528 o similar) | 3 | Dos para la cabeza y uno de repuesto (o para el extra "panel fijo"). |
| Resistencias de 10 kΩ | 3 | Divisores de los LDR (la tercera, para el extra "panel fijo"). |
| Mini protoboard de 170 puntos | 1 | Va pegada sobre el brazo del servo y lleva los LDR. |
| Fuente de 5 V para el servo (**MB102** solo si alcanza la corriente pico) | 1 | Alimentación aparte para el servo; elegirla en el issue #09. |
| Capacitor electrolítico de 470–1000 µF, 10 V o más | 1 | Absorber los picos de corriente del servo. |
| Pulsadores | 4 | Botones. |
| Cables dupont macho-macho largos (20 cm) | ~10 | Conectar la mini protoboard que gira, y un cable corto para el autotest del DAC. |
| Cartón y cinta doble faz | — | Pared entre los LDR y fijación del servo. |
| Linterna (sirve la del celular) | — | La "luz" a seguir. |

#### 3.2 Circuitos

```text
LDR (en la mini protoboard, ×2)              SERVO

3,3 V de la LPC ── LDR ──┬── P0.23 / P0.24   cable marrón  ── GND (común)
                         └── 10 kΩ ── GND     cable rojo    ── +5 V fuente   ─┬─ 470 µF ── GND
                                              cable naranja ── P2.0 (PWM1.1)

DAC: MONITOR Y AUTOTEST                      BOTONES

P0.26 (AOUT) ──┬── punta del osciloscopio    pin ── pulsador ── GND
               └── cable ── P0.25 (AD0.2)    (pull-up interno de la LPC1769,
                                              sin resistencias)
```

- **LDR:** con más luz, baja su resistencia y sube la tensión en el pin. Alimentar los divisores con los **3,3 V de la placa**, que es la misma referencia del ADC; así la lectura no depende de la fuente del servo y nunca supera el rango del ADC.
- **Servo:** confirmar en la ficha del modelo elegido que reconoce la señal de 3,3 V. Alimentarlo desde una **fuente de 5 V capaz de entregar su corriente pico**, no desde la placa ni desde el adaptador USB: en cada arranque consume picos que pueden resetear la placa. El capacitor va cerca del servo.
- **DAC:** no maneja ninguna carga. La salida va solo a la punta del osciloscopio y, por un cable, a un canal libre del ADC para el autotest. Sin componentes, no hay nada que se pueda quemar o fallar.
- **Masa común** entre la placa, la fuente del servo y el adaptador USB-UART.

#### 3.3 Mapa de pines provisional

Se verifica contra el esquemático de la placa en el issue #10.

| Señal | Pin | Función |
| --- | --- | --- |
| LDR izquierdo | P0.23 | AD0.0 |
| LDR derecho | P0.24 | AD0.1 |
| Lectura del DAC (autotest) | P0.25 | AD0.2, unido por un cable a P0.26 |
| LDR fijo *(extra)* | P1.31 (a confirmar en la placa) | AD0.5 |
| Servo | P2.0 | PWM1.1 |
| Monitor (osciloscopio) | P0.26 | AOUT (DAC) |
| UART TX / RX | P0.2 / P0.3 | TXD0 / RXD0 |
| Botón modo | P2.11 | EINT1 |
| Botón barrido / baile | P2.12 | EINT2 |
| Botones manual izquierda / derecha | P2.6 / P2.7 | GPIO con interrupción (vector EINT3) |
| LED RGB de estado *(extra)* | P0.4 / P0.5 / P0.6 (a confirmar) | GPIO |
| CPU activa (medición) | P0.22 | GPIO; es el LED de la placa |

**No usar P2.10 (EINT0) para un botón:** si está en bajo al arrancar, la placa entra en modo ISP. P0.26 queda tomado por AOUT, así que no se puede usar AD0.3.

#### 3.4 Montaje

- **Servo:** acostado con el eje hacia arriba, pegado con cinta doble faz a una base firme (un taco de madera o una cajita). Si la base se mueve, el girasol no sigue bien.
- **Cabeza:** la mini protoboard de 170 puntos, pegada con doble faz sobre el brazo del servo.
- **LDR:** uno a cada lado de la **canaleta central** de la mini protoboard, a 1–2 cm entre sí, con las patas un poco dobladas hacia adelante. Las resistencias de 10 kΩ van en la misma mini protoboard.
- **Pared:** un rectángulo de cartón de ~3 × 3 cm **encastrado en la canaleta central**, entre los dos LDR. Es lo que crea la diferencia de luz; sin ella, los dos miden casi lo mismo.
- **Cables:** 4 cables largos con comba (3,3 V, GND, LDR izquierdo, LDR derecho) hasta la protoboard principal. Limitar por software el recorrido a lo que permitan los cables.
- **Impresión 3D:** no hace falta. Opcional, como extra estético: una maceta que esconde el servo (sección 6).

### 4. Calibración (una vez, con el hardware armado)

1. **Rango del servo:** 1–2 ms es el estándar teórico, pero muchos SG90 necesitan ~0,5–2,4 ms para recorrer 180°. Probar valores de `MR1` y anotar `PULSO_MIN` y `PULSO_MAX`, sin forzar los topes mecánicos.
2. **Diferencia entre LDR:** dos LDR nunca son idénticos. Con la misma luz en los dos (sin linterna), medir `izq − der` y guardarlo como `OFFSET`. El comando `CAL` lo hace automáticamente.
3. **Sentido de giro:** si el girasol se aleja de la luz en vez de acercarse, invertir el signo en el control (o intercambiar los cables de los LDR).

### 5. Firmware

Se usa la biblioteca de drivers de la cátedra (`LPC17xx-CMSIS-Driver-Enhancement`) o registros CMSIS directos donde convenga, encapsulados detrás de los módulos de la sección 5.3.

#### 5.1 Tiempos

| Parámetro | Valor inicial | Comentario |
| --- | --- | --- |
| CCLK / PCLK | 100 MHz / 25 MHz | Configuración por defecto de `SystemInit` con cristal de 12 MHz; confirmar. |
| ADC | Burst, canales 0 y 1 | Con `CLKDIV = 255`, el reloj del ADC es ~97,6 kHz y cada conversión en burst lleva 64 ciclos: ~1500 conversiones/s en total, ~760 por canal. |
| Bloque de DMA | 30 palabras ≈ 20 ms | Ping-pong; 30 sirve tanto para 2 como para 3 canales. Promediar 20 ms cancela el parpadeo de 100 Hz de las luces del aula. |
| Timer0 | 1 cuenta = 1 µs (`PR = 24`), `MR0 = 20000` | Interrupción cada 20 ms: un paso de control. |
| PWM1 (servo) | 1 cuenta = 1 µs (`PR = 24`), `MR0 = 20000` | `MR1 = PULSO_MIN … PULSO_MAX` (~1000–2000). Resolución: ~0,1–0,2° por cuenta. |
| Paso de control | 5 µs por tick | ~250 µs/s, unos 45°/s. Ajustable por comando. |
| DAC (monitor) | Se actualiza en cada paso de control (50 Hz) | 10 bits sobre 0–3,3 V: ~3,2 mV por paso. |
| Telemetría | 10 tramas/s, 115200 8N1 | ~10 % de la capacidad del enlace. |
| Barrido | 1° cada 40 ms | ~7 s de punta a punta. |

#### 5.2 Detalles verificados en el manual (UM10360)

- **ADC en burst:** recorre los canales habilitados en `SEL`, del menor al mayor. Con `BURST = 1`, los bits START deben quedar en 000 y **`ADGINTEN` debe ser 0**: los pedidos salen de los bits `ADINTENn` de cada canal (cap. 29).
- **ADC con DMA:** el pedido de DMA sale de la línea de interrupción del ADC, que debe quedar **deshabilitada en el NVIC**. Burst size del DMA = 1 (sec. 29.6.4). La biblioteca de la cátedra usa `ADGDR` como origen. Cada palabra trae el resultado en los bits 15:4 y el **número de canal en los bits 26:24 (campo CHN)**, así que el firmware separa las muestras de cada LDR leyendo ese campo.
- **A verificar en la placa (issue #12):** que con varios canales cada conversión genere exactamente una transferencia y no se pierdan ni dupliquen muestras. **Plan B:** ISR del ADC con `ADINTEN` del último canal, leyendo `ADDR0`/`ADDR1`.
- **DMA en Sleep:** el GPDMA **no puede acceder a la flash ni a la SRAM principal** en modo Sleep, solo a la **AHB SRAM** (0x2007C000) y a los periféricos (sec. 4.8.1). Todos los buffers y LLI del DMA van en AHB SRAM, con un atributo de sección del linker.
- **DAC:** AOUT es la función 10 de P0.26 (`PINSEL1`, bits 21:20). El valor va en los bits 15:6 de `DACR` y la salida es `VALUE × 3,3 V / 1024`. Con `BIAS = 0` asienta en 1 µs como máximo (hasta 700 µA); con `BIAS = 1`, en 2,5 µs (350 µA) (cap. 30).
- **Autotest y burst:** el autotest se hace al arrancar, con el ADC en modo por software (START = 001 y un solo canal en `SEL`). Recién después se configura el burst, con START = 000.
- **PWM:** `MR0` fija el período para **todos** los canales de PWM1. Los cambios de `MR1` se aplican al habilitarlos en `LER`, al inicio del período siguiente, sin pulsos cortados.
- **Interrupciones GPIO:** comparten el canal de EINT3 (cap. 6); la ISR revisa `IO2IntStatF` para saber qué botón fue.

#### 5.3 Módulos

| Módulo | Interfaz propuesta | Responsabilidad |
| --- | --- | --- |
| `board` | `board_init()` | Relojes, `PINSEL`/`PINMODE`, pines de depuración. |
| `tick` | `tick_init()`, `tick_ms()` | SysTick de 1 ms, antirrebote, avisos periódicos. |
| `ctrl_timer` | `ctrl_timer_init(periodo_us)` | Timer0: aviso de "paso de control" cada 20 ms. |
| `adq` | `adq_init(canales)`, `adq_get(&izq, &der, &fijo)`, `adq_overruns()` | ADC burst + DMA ch0 ping-pong; separa canales por el campo CHN y promedia por bloque. |
| `servo` | `servo_init()`, `servo_set_us(pulso)`, `servo_enable(on)` | PWM1.1 a 50 Hz con actualización por `LER`; límites `PULSO_MIN`/`PULSO_MAX`. |
| `monitor` | `monitor_init()`, `monitor_elegir(variable)`, `monitor_actualizar(&estado)` | Escala la variable elegida a 0–1023 y la escribe en el DAC (sección 5.7). |
| `autotest` | `autotest_adc() → ok` | DAC → cable → AD0.2 con tres tensiones conocidas; informa el resultado. |
| `link` | `link_send(buf, len)`, `link_get_line(buf)` | UART0: TX por DMA ch1 sin bloquear, RX por interrupción hasta `\n`. |
| `buttons` | `buttons_init()`, `buttons_take_events()` | EINT1, EINT2 e interrupciones GPIO, con antirrebote por SysTick. |
| `power` | `power_sleep()`, `power_cpu_pct()` | `__WFI()` y medición del tiempo dormido. |
| `girasol` *(app)* | `girasol_step(izq, der) → pulso` | Modos, estados, calibración y paso de control. **No toca registros.** |
| `commands`, `telemetry` *(app)* | `commands_execute(linea)`, `telemetry_build(buf, estado)` | Protocolo de la sección 8. |

#### 5.4 Prioridades de interrupción

| Fuente | Prioridad | Trabajo en la ISR |
| --- | --- | --- |
| DMA (`DMA_IRQn`, un vector para los 8 canales) | 0 (más alta) | Ver en `DMACIntTCStat` qué canal terminó; marcar bloque listo. |
| Timer0 | 1 | Marcar "toca paso de control". |
| UART0 | 2 | Guardar bytes recibidos; marcar línea completa. |
| SysTick | 3 | Contar ms, antirrebote, aviso de telemetría. |
| EINT1, EINT2, EINT3/GPIO | 4 | Registrar el evento y deshabilitar el botón hasta que venza el antirrebote. |

Las ISR solo levantan banderas; el trabajo se hace en `main`.

#### 5.5 Lazo principal

```c
estado.autotest_ok = autotest_adc();   /* antes de arrancar el burst */
adq_init(CANALES_LDR);

while (1) {
    if (ctrl_tick_pendiente()) {
        adq_get(&izq, &der, &fijo);
        servo_set_us(girasol_step(izq, der));
        monitor_actualizar(&estado);
    }
    if (link_get_line(linea))   { commands_execute(linea); }
    if (tick_telemetry_due())   { telemetry_build(trama, &estado); link_send(trama, len); }
    procesar_botones(buttons_take_events());

    __disable_irq();               /* evita perder una interrupción entre la consulta y el WFI */
    if (!hay_trabajo_pendiente()) {
        power_sleep();             /* __WFI(); despierta igual con la IRQ pendiente */
    }
    __enable_irq();
}
```

Dejar un `#define` para desactivar Sleep durante la depuración, porque puede cortar la conexión con el debugger.

#### 5.6 Control

```c
int dif = (izq - der) - OFFSET;
if (modo == VAMPIRO) dif = -dif;          /* extra: huye de la luz */
if (dif >  MARGEN) pulso += PASO;         /* gira hacia la izquierda */
if (dif < -MARGEN) pulso -= PASO;         /* gira hacia la derecha  */
if (pulso < PULSO_MIN) pulso = PULSO_MIN;
if (pulso > PULSO_MAX) pulso = PULSO_MAX;
```

- **`MARGEN` (zona muerta):** si la diferencia es chica, no se mueve. Evita que el servo tiemble yendo y viniendo.
- **`PASO`:** grande es rápido pero nervioso; chico es suave pero lento. Se ajusta mirando la web.
- Como el servo corrige de a pasos 50 veces por segundo, el movimiento se ve continuo. No hace falta PID.

#### 5.7 DAC: monitor y autotest

**Monitor.** En cada paso de control, la CPU escribe en el DAC la variable elegida (comando `MON` o selector de la web), escalada a 0–1023:

| Variable | Escala | Qué se ve en el osciloscopio |
| --- | --- | --- |
| `ERROR` *(por defecto)* | `512 + dif / 8`, limitado a 0–1023: **1,65 V es error cero** | Al mover la linterna, el error salta y vuelve a 1,65 V de a escalones, a medida que el servo corrige. Es la realimentación en vivo. |
| `ANG` | `(pulso − PULSO_MIN) × 1023 / (PULSO_MAX − PULSO_MIN)` | La trayectoria del servo: rampas mientras sigue la luz y planos cuando llega. |
| `IZQ` / `DER` | `valor_adc >> 2` | La luz que mide cada LDR. |

Como el DAC cambia cada 20 ms, el osciloscopio se usa con base de tiempo lenta (0,5–1 s/div) o en modo *roll*. El pulso del servo (P2.0) se mira aparte, con ~5 ms/div.

**Autotest al arrancar.** Antes de habilitar el burst:

1. El DAC saca tres valores conocidos, por ejemplo 128, 512 y 896 (~0,41 V, 1,65 V y 2,89 V).
2. Para cada uno, el ADC lee AD0.2 por software (promedio de 8 lecturas). Como el DAC es de 10 bits y el ADC de 12, se espera `cuentas ≈ valor_dac × 4`.
3. Si las tres lecturas caen dentro de ±2 % de la escala (~±80 cuentas), el autotest pasa. Si falla, el LED de la placa parpadea rápido y la telemetría informa `"autotest":"FALLA"`, pero el girasol funciona igual.

Para mostrar que el autotest sirve, se desconecta el cable y se reinicia la placa: tiene que informar `FALLA`.

### 6. Modos, estados y pavaditas

**Etapa 1 (lo mínimo para aprobar):**

| Modo / estado | Qué hace |
| --- | --- |
| **AUTOMÁTICO** | Sigue la luz (sección 5.6). |
| **MANUAL** | Los botones izquierda/derecha o la web mueven el servo; no sigue la luz. Sirve para mostrar la diferencia con el automático. |
| **BARRIDO** | Recorre de punta a punta, guarda la luz total en cada ángulo (la web dibuja un "radar") y se queda en el ángulo más iluminado. Después vuelve a AUTOMÁTICO. |

**Etapa 2 (pavaditas, en orden de costo):**

| Pavadita | Qué agrega | Costo |
| --- | --- | --- |
| **Modo vampiro** | Huye de la luz: el mismo control con el signo invertido. En la demo, cambiar de girasol a vampiro con un botón. | Una línea de código. |
| **Se duerme de noche** | Si todo está oscuro varios segundos, va al centro, apaga el pulso del servo (queda suelto y no consume), baja el control a 2 Hz y casi no sale de Sleep. Con luz, "amanece": hace un barrido y vuelve a seguir. La web muestra cómo cae el % de CPU. | Código; buen argumento de bajo consumo. |
| **Baile** | Con el botón o desde la web, hace un vaivén de lado a lado. | Código. |
| **Repetición de la señal cruda** | Un modo más del monitor: la CPU convierte un bloque de muestras del ADC al formato del DAC (bits 15:6) en AHB SRAM, y el DMA canal 2 lo reproduce con el contador del DAC (`DACCNTVAL`) al mismo ritmo con el que se muestreó. En el osciloscopio se ve la señal reconstruida, con los escalones del muestreo. | Código; suma DMA al DAC. |
| **Girasol vs. panel fijo** | Un tercer LDR fijo, mirando hacia arriba (un "panel solar" quieto). La web acumula la luz que captó cada uno y muestra cuánto más junta el girasol. Unidades relativas, porque el LDR no es lineal. | 1 LDR + 1 resistencia; un canal más de ADC por DMA. |
| **LED RGB de estado** | Color según el estado: siguiendo, buscando, durmiendo, vampiro. Va por **GPIO** (colores fijos), no por PWM, porque PWM1 comparte el período de 20 ms del servo y a 50 Hz el LED parpadearía. | 1 LED RGB + 3 resistencias. |
| **Flor y maceta impresas** | Cabeza con forma de girasol (con la pared entre los LDR incluida) y maceta que esconde el servo. Los STL están en `cad/` y se regeneran con `cad/generar_girasol.py` si cambian las medidas. | Estético; impresión. |

### 7. Prueba física inicial (después del software y de elegir componentes)

1. **LDR (~20 min, sin programar):** dos LDR y dos resistencias de 10 kΩ en la protoboard, con la pared de cartón en el medio. Medir con multímetro la tensión de cada uno mientras se mueve la linterna de un lado al otro. **Pasa** si uno sube y el otro baja de forma clara (más de unos cientos de mV).
2. **Servo (~20 min):** PWM1 con `PR = 24` y `MR0 = 20000`; probar `MR1` en 1000, 1500 y 2000. **Pasa** si el brazo va a tres posiciones distintas sin temblar.
3. **DAC (opcional, ~10 min):** escribir 0, 512 y 1023 en el DAC y medir P0.26 con el multímetro. **Pasa** si da ~0 V, ~1,65 V y ~3,3 V.

Si las dos primeras pasan, el proyecto es viable y el resto es programar.

### 8. Protocolo UART

**115200 baudios, 8N1, líneas terminadas en `\n`.**

**Placa → PC:** una trama JSON cada 100 ms.

```json
{"v":1,"t":123456,"izq":2140,"der":2095,"fijo":1800,"pulso":1530,"ang":95,"modo":"AUTO","estado":"SIGUIENDO","mon":"ERROR","autotest":"OK","cpu":6,"ovr":0}
```

| Campo | Significado |
| --- | --- |
| `v` | Versión del protocolo. |
| `t` | ms desde el arranque. |
| `izq`, `der`, `fijo` | Promedios del último bloque, 0–4095 (`fijo` es `null` si no está el extra). |
| `pulso`, `ang` | Pulso del servo en µs y ángulo estimado según la calibración. |
| `modo` | `AUTO`, `MANUAL` o `VAMPIRO`. |
| `estado` | `SIGUIENDO`, `BARRIDO`, `DURMIENDO` o `BAILANDO`. |
| `mon` | Variable que sale por el DAC: `ERROR`, `ANG`, `IZQ` o `DER`. |
| `autotest` | Resultado del autotest del arranque: `OK` o `FALLA`. |
| `cpu` | % de tiempo con la CPU activa en el último segundo. |
| `ovr` | Bloques de ADC perdidos (debe quedar en 0). |

Durante el barrido se agrega `"bar":[ang,luz]` en cada paso, para que la web arme el radar.

**PC → placa:**

| Comando | Efecto |
| --- | --- |
| `MODO AUTO` / `MODO MANUAL` / `MODO VAMPIRO` | Cambia el modo. |
| `ANG <grados>` | Mueve el servo (en modo manual). |
| `BARRIDO` / `BAILE` | Arranca el barrido o el baile. |
| `PASO <µs>` / `MARGEN <n>` | Ajusta el control. |
| `CAL` | Calibra `OFFSET` con la luz actual. |
| `MON ERROR` / `MON ANG` / `MON IZQ` / `MON DER` | Elige qué variable sale por el DAC hacia el osciloscopio. |

Cada comando se responde con `{"ack":"<comando>","ok":true}`, o `"ok":false` si es inválido. El girasol sigue funcionando aunque se desconecte la PC.

### 9. Página web

Página estática (`front/index.html` + `app.js` + `style.css`) que lee el puerto serie con la **Web Serial API** de Chrome/Edge; sin servidor. Se sirve desde `localhost` (por ejemplo, `python -m http.server`).

- **Girasol en vivo:** semicírculo con el brazo en el ángulo actual y dos barras con la luz de cada LDR.
- **Radar del barrido:** gráfico polar de luz según ángulo.
- **Girasol vs. panel fijo** *(extra)*: luz acumulada por cada uno.
- **Controles:** modos, ángulo manual, barrido, baile, `PASO`, `MARGEN`, calibrar y selector de la variable del monitor.
- **Diagnóstico:** resultado del autotest, % de CPU, overruns, estado de la conexión.
- **Exportar CSV** para el informe.
- **Modo simulación:** un modelo simple del girasol y la linterna para desarrollar la página sin la placa.

Copiar las bibliotecas que se usen dentro de `front/lib/` para que la demo funcione sin internet.

### 10. Plan de validación por periférico

| Periférico | Prueba | Evidencia esperada |
| --- | --- | --- |
| ADC | Tensiones conocidas (divisor o potenciómetro) en P0.23 y P0.24. | Tabla: cuentas ≈ 4095·V/3,3 en cada canal. |
| ADC + DMA | Correr 10 minutos con los dos canales. | Muestras de cada canal separadas correctamente por CHN; ~50 bloques/s; `ovr = 0`. |
| Timer0 | Conmutar un pin en cada paso de control. | Osciloscopio: cuadrada de 25 Hz (un flanco cada 20 ms). |
| PWM | `MR1` en `PULSO_MIN`, 1500 y `PULSO_MAX`. | Osciloscopio: período de 20 ms y ancho correcto; sin glitches al cambiar. |
| DAC (monitor) | Monitor en `ERROR`; mover la linterna de un lado al otro. | Osciloscopio en P0.26 (modo *roll*): el error salta y vuelve a 1,65 V de a escalones. |
| DAC (autotest) | Arrancar con el cable puesto y después sin el cable. | `autotest: OK` con cable; `FALLA` y LED parpadeando sin cable. |
| UART | Tramas + un comando válido y uno inválido. | 10 tramas/s; el inválido se rechaza sin afectar el control. |
| EINT / GPIO | 50 pulsaciones de cada botón. | El contador da 50. |
| Sleep | P0.22 activo solo mientras la CPU trabaja; comparar AUTOMÁTICO con DURMIENDO. | El % de CPU del osciloscopio coincide con `cpu`; en DURMIENDO cae claramente. |
| Sistema | Linterna en distintas posiciones, manual vs. automático. | Video o CSV: en automático el ángulo sigue a la luz; en manual no. |

### 11. División de trabajo y backlog para dos

El plan de issues actualizado está en [PLAN_ISSUES.md](PLAN_ISSUES.md). Ordena el trabajo en dos fases: software compilable y demostrable con datos simulados; después elección eléctrica de componentes, montaje, mediciones y calibración. Cada issue tiene responsable, dependencia y criterio de cierre. Los periféricos solo se validan después de medirlos en la placa.

### 12. Decisiones abiertas

- Confirmar con la cátedra los periféricos obligatorios y que el uso del DAC (monitor + autotest) y el control "paso a paso" alcancen. Si piden más, sumar la repetición de la señal cruda con DMA (sección 6).
- `PULSO_MIN` / `PULSO_MAX` del servo comprado.
- `PASO`, `MARGEN` y `OFFSET`, medidos con el montaje real.
- Si el ADC con DMA de varios canales funciona como se espera (#12) o se pasa al plan B.
- Qué pavaditas entran, según el tiempo disponible.

### 13. Fuentes técnicas

- **UM10360 — LPC176x/5x User manual** (copia local: `LPC176x_5x User manual (para imprimir).pdf`): cap. 4 (sec. 4.8.1, Sleep y GPDMA), cap. 6 (NVIC), cap. 9 (GPIO), cap. 21 (Timers), cap. 23 (SysTick), cap. 29 (ADC; burst, `ADGDR` y sec. 29.6.4), cap. 30 (DAC), cap. 31 (GPDMA), y los capítulos de PWM1 y UART.
- [NXP: hoja de datos LPC1769](https://www.nxp.com/docs/en/data-sheet/LPC1769_68_67_66_65_64_63.pdf).
- Biblioteca de drivers de la cátedra: carpeta `LPC17xx-CMSIS-Driver-Enhancement` (en `lpc17xx_gpdma.c`, el origen del DMA para el ADC es `ADGDR`).
- [MDN: Web Serial API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API).

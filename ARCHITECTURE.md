# TP final: medición y control de posición con un transformador lineal

**Estado:** propuesta técnica para desarrollar y validar en el laboratorio. Las cifras marcadas como iniciales son puntos de partida, no resultados medidos. Placa confirmada: **LPC1769**. Fecha: noviembre de 2026.

## 1. Qué vamos a hacer

Desarrollar un sistema que mida y controle la posición de un núcleo de ferrita dentro de un tubo. La LPC1769 generará con el DAC una señal senoidal para excitar la bobina primaria; una etapa analógica acondicionará la señal inducida en **una única bobina secundaria** para leerla con el ADC. La posición deseada se ingresará desde una página web por UART y un servomotor moverá el núcleo mediante el mecanismo impreso. La medición se usará como realimentación para detener el movimiento cerca de la posición solicitada.

**Alcance acordado:** se usa el primario y un solo secundario. El otro secundario existente queda sin conectar. A diferencia de un LVDT diferencial convencional, en esta versión no se comparan ambos secundarios; la posición se obtendrá a partir de la amplitud del secundario elegido y de una calibración experimental.

## 2. Requisitos de la materia

| Requisito | Aplicación en el proyecto |
| --- | --- |
| Sensor | Transformador de posición con núcleo de ferrita, primario y un secundario utilizado. |
| Actuador | Servomotor SG90 con mecanismo para mover el núcleo. |
| DAC | Generar la excitación senoidal del primario. |
| ADC | Medir la envolvente acondicionada del secundario. |
| DMA | Repetir la tabla de muestras hacia el DAC; también se propone usarlo para adquirir bloques del ADC. |
| Comunicación serie | UART entre la LPC1769 y la computadora mediante un puerto USB-serie. |
| Interfaz | Página web para conectar el puerto, ingresar la posición y observar mediciones y estado. |
| Acondicionamiento analógico | Etapa para excitar el primario y detector de envolvente con diodo, filtro y adaptación al ADC. |
| Lazo de control | Comparar posición deseada y medida y accionar el servo hasta reducir el error. |

Los requisitos fotografiados están en [requisitos.png](docs/requisitos.png).

## 3. Vista general

```text
                          señal de excitación: 1 kHz / 1 V RMS en el primario
LUT de seno en RAM → DMA → DAC → adaptación / amplificación → BOBINA PRIMARIA
                                                                    ║
                                                        núcleo de ferrita móvil
                                                                    ║
BOBINA SECUNDARIA → diodo + filtro de envolvente → adaptación → ADC → DMA → RAM
                                                                       │
                         calibración: nivel medido → posición estimada  │
                                                                       ▼
Web en PC ←──── USB-serie / UART ────→ comandos + telemetría ←──── CONTROL
  │                                                                │
  └─ posición solicitada ─────────────────────────────────────────►│
                                                                   ▼
                                                        PWM → servo → mecanismo
                                                                        │
                                         mueve la ferrita ◄─────────────┘
```

El DAC, el ADC, el PWM y la UART son periféricos distintos. La web muestra datos y envía la consigna; **el lazo de control se ejecuta en la LPC1769**, de modo que el navegador no tiene que tomar decisiones de movimiento en cada muestra.

## 4. Datos conocidos y decisiones iniciales

| Concepto | Valor o decisión | Estado |
| --- | --- | --- |
| Microcontrolador | LPC1769 | Confirmado. |
| Excitación en el primario | Senoidal de 1 kHz y **1 V RMS** | Objetivo indicado en el pizarrón; medir en la bobina conectada. |
| Bobinas | Un primario y un secundario | Confirmado. |
| Medición | Detector de pico/envolvente con diodo y lectura por ADC | Idea propuesta con el profesor; dimensionar y probar. |
| Frecuencia inicial de ADC | 8.000 muestras por segundo de la **envolvente acondicionada** | Propuesta inicial; validar ADC, filtrado, ruido y respuesta real. |
| Promedio inicial | Bloques de 64 muestras: resultado nuevo cada 8 ms | Propuesta inicial; no significa que el sensor responda en 8 ms. |
| Control del servo | Actualización inicial cada 20 ms | Propuesta: verificar servo y mecánica. |
| Telemetría | Cada 100 ms, inicialmente | Propuesta, configurable. |
| Tubo / núcleo | Tubo de unos 200 mm y núcleo de unos 100 mm | Medidas aproximadas: confirmar antes de fijar carrera. |
| Carrera deseada | Aproximadamente 50 mm a cada lado del centro | Objetivo; los 100 mm totales nominales no dejan margen en un tubo de 200 mm con núcleo de 100 mm. |
| Modelo de servo y adaptador | SG90 y brazo original | Prototipo; ángulo, fuerza, anclajes y acople a ferrita deben verificarse. |

**Decisión de muestreo revisada:** usar inicialmente **8.000 muestras/segundo** y promediar bloques de 64, uno cada 8 ms. A 2.000 muestras/s la portadora de 1 kHz queda justo en Nyquist: si aparece rizado de 1 kHz en el detector, la lectura puede depender de la fase. A 8.000 muestras/s hay ocho muestras por ciclo nominal y más margen para observar y promediar el rizado; la capacidad nominal de la LPC1769 es de hasta 200 kconversiones/s, pero debe verificarse el disparo y el DMA reales. **Esto no reemplaza el filtro analógico ni garantiza exactitud**: el rizado, los armónicos, el ruido sobre 4 kHz, el aliasing y la deriva entre relojes deben medirse y limitarse. No promediar automáticamente 2.000 muestras: a 8 kHz una ventana así duraría 250 ms y retrasaría el control. Una ventana de 64 contiene nominalmente ocho ciclos de portadora, no necesariamente ciclos completos exactos con tolerancias o con una envolvente que cambia.

## 5. Funcionamiento por etapas

### 5.1 Excitación de la bobina primaria

1. Preparar una tabla de una senoidal de 32 muestras como valor inicial.
2. El DMA transfiere la tabla al DAC; el contador del DAC marca la frecuencia de actualización y una lista enlazada permite repetirla sin interrupción por muestra.
3. Ejemplo: `32 muestras/ciclo × 1.000 ciclos/s = 32.000 actualizaciones/s`. Comprobar el reloj y el divisor efectivos, la continuidad entre ciclos y el formato requerido por el registro del DAC.
4. Adaptar la señal con una etapa analógica capaz de manejar la bobina. Medir con osciloscopio la tensión **entre los terminales del primario**, con la bobina conectada.

**Elección para este proyecto:** generar en el DAC una senoidal de **0,5 V RMS**, centrada a mitad de la referencia, y utilizar una etapa externa de ganancia ajustable cercana a **2** para alcanzar **1 V RMS en la bobina primaria**. Así se conserva margen en los extremos del DAC y se separa la generación de la potencia requerida por la bobina. La especificación de 1 V RMS sigue siendo para la señal alterna medida **en el primario conectado**, no una modificación del objetivo.

Con `VREFN = 0 V` y `VREFP = 3,3 V`, valores nominales que se deben medir en la placa:

| Parámetro | Valor elegido |
| --- | --- |
| DAC / resolución | AOUT `P0.26`, 10 bits, códigos 0 a 1023. |
| Frecuencia de la onda | 1.000 Hz. |
| Tabla senoidal | 32 muestras por ciclo; 32.000 actualizaciones/s. |
| Centro digital | Código **512**, equivalente a unos **1,65 V**. |
| Amplitud inicial | **219 cuentas pico**: códigos aproximadamente **293 a 731**. |
| Señal ideal del DAC | Aproximadamente **0,5 V RMS de componente alterna**, o 1,414 V pico a pico, superpuesta a 1,65 V de continua; extremos aproximados 0,944 a 2,356 V. |
| Etapa externa | Ganancia inicial de tensión aproximada **2**, con capacidad de corriente y acoplamiento a la bobina adecuados. |
| Objetivo en el primario | **1 V RMS de alterna**, equivalente a 1,414 V pico y 2,828 V pico a pico si la señal es senoidal y no tiene continua. |

La LUT se calcula inicialmente como `codigo[n] = 512 + redondear(219 × sen(2πn/32))`, con `n` de 0 a 31. Para este DAC la conversión ideal usa `VREFN + codigo × (VREFP − VREFN) / 1024`. Si DMA escribe el registro completo, cada palabra debe tener el código ubicado en `DACR[15:6]` y el valor de `BIAS` elegido; el contador del DAC y el canal DMA se configuran según el reloj periférico real. Con 32 kactualizaciones/s el intervalo nominal entre muestras es 31,25 µs; el DAC dispone de margen de velocidad para este punto de partida. Medir la frecuencia real y verificar que la tabla se repite sin discontinuidades.

La ruta elegida es `DAC → adaptación de continua y amplificación de tensión ×2 → etapa capaz de entregar la corriente requerida → desacople de continua → primario`. La etapa externa debe tener suficiente margen de entrada/salida y alimentarse según el circuito elegido; si se usa una alimentación simple, requiere una polarización adecuada. El desacople evita imponer la componente continua del DAC a la bobina. **No conectar la bobina directamente al DAC:** primero medir su resistencia y, si es posible, su impedancia a 1 kHz; con ella se dimensionan el amplificador de salida, su fuente y el capacitor de acoplamiento. No están definidos todavía un integrado ni valores definitivos de componentes porque faltan esos datos.

Como ejemplo, si se midiera una carga puramente resistiva de 50 Ω, 1 V RMS exigiría 20 mA RMS (unos 28 mA pico); la bobina real es inductiva y su impedancia a 1 kHz puede ser diferente. Medir 1 V RMS **entre los dos terminales del primario**, con la bobina conectada, y ajustar la ganancia externa si hace falta. Verificar con el profesor la conexión de masa y las puntas del osciloscopio antes de medir, especialmente si algún terminal no está a masa. La amplitud depende de la referencia, la etapa y la carga: los códigos no garantizan el valor en la bobina.

### 5.2 Secundario y acondicionamiento

El secundario produce una señal alterna cuya amplitud depende de la posición del núcleo. La propuesta es:

```text
secundario → rectificación con diodo → capacitor + resistencia de descarga
           → buffer/ganancia/protección según mediciones → entrada ADC
```

- El diodo y el capacitor siguen la envolvente; **la resistencia de descarga es necesaria** para que el nivel también baje cuando el núcleo cambia de posición. No confundir con un detector que retiene indefinidamente el máximo.
- La caída del diodo depende de la corriente y puede ser comparable a la señal. Primero medir el secundario; considerar diodo Schottky o rectificador de precisión con operacional si hace falta. La calibración no recupera una señal demasiado pequeña para superar el umbral ni una etapa saturada.
- Elegir `R` y `C` luego de medir amplitud, carga, ruido y velocidad requerida. `τ = R × C` determina el compromiso entre rizado y tiempo de respuesta. Como experimento inicial se puede evaluar un valor de `τ` del orden de 5–20 ms, **no definitivo**. Verificar que la salida baje y suba adecuadamente y observar el rizado real.
- Acondicionar la señal para que el ADC nunca reciba tensiones negativas ni supere su referencia/alimentación y límites eléctricos. Revisar también fuente, impedancia de entrada, protección, polarización y referencia de masa de los instrumentos. Usar buffer si la carga de la etapa siguiente cambia la medición.
- El filtrado analógico debe reducir la componente residual de 1 kHz y otras frecuencias que puedan plegarse al muestrear a 8 kHz. Una adquisición sincronizada puede sesgar lecturas si queda rizado: observarla y comprobarlo; no confiar únicamente en el promedio digital.

No está definida todavía una lista de valores ni un esquema eléctrico listo para conectar: dependen de las mediciones y de la alimentación del operacional seleccionado. Documentar con el profesor el circuito final y sus referencias.

### 5.3 ADC, DMA y promedio

1. Configurar un solo canal ADC para la salida acondicionada. Propuesta inicial: disparar a **8 kHz** con un Timer Match compatible y verificar el ritmo real. En la LPC1769 el ADC también puede operar en burst, pero burst por sí solo **no** fija 8 kHz.
2. Configurar DMA para copiar los resultados a buffers de 64 muestras; utilizar doble buffer o un mecanismo equivalente que garantice que la CPU no procese memoria que el DMA está sobrescribiendo.
3. En la interrupción de DMA, reconocer y limpiar la causa y publicar el buffer terminado. El promedio, la conversión y la calibración se ejecutan fuera de la ISR.
4. Extraer los 12 bits válidos y revisar los flags de conversión/overrun según el registro usado. **El tamaño del registro DMA no se debe confundir con el tamaño del valor ADC:** verificar ancho y alineación en el manual y el driver.
5. Calcular el promedio de 64 valores con un acumulador suficientemente grande. Publicar el valor en cuentas de ADC, su equivalente en tensión de entrada y una marca de tiempo.

En la LPC1769, verificar la ruta específica ADC→DMA, las condiciones de solicitud del ADC y la configuración de interrupciones: **no habilitar a la vez un consumidor CPU que lea el mismo dato y el DMA sin coordinarlo**. En la configuración ADC→DMA descrita por el manual, la solicitud DMA depende de las condiciones de interrupción del ADC, pero la IRQ del ADC se deja deshabilitada en el NVIC; distinguir esto de deshabilitar la condición que genera la solicitud DMA. Con disparo por Timer Match, revisar el flanco configurado; si el match alterna la salida, un flanco del mismo tipo ocurre cada dos cambios. Comprobar que la conversión terminó antes del próximo disparo y registrar errores o pérdida de datos.

Punto de control: alimentar el ADC con tensiones conocidas dentro del rango y verificar muestra, promedio, frecuencia, buffers y ausencia de overrun antes de conectar la cadena completa.

### 5.4 De nivel a posición: calibración obligatoria

Con un solo secundario y un detector de amplitud, la relación entre voltaje y posición **puede no ser lineal ni única**: dos posiciones distintas podrían producir el mismo nivel. El detector de envolvente no conserva el signo de fase. No se puede afirmar de antemano que el sistema distinga ambos lados ni convertir voltios a centímetros con una fórmula genérica.

**Procedimiento propuesto:**

1. Con el control automático deshabilitado, ubicar un cero mecánico de referencia y confirmar que el núcleo puede desplazarse sin trabarse.
2. Excitar el primario en las condiciones definitivas. Mover el núcleo manualmente o con pasos pequeños en modo de prueba; medir su posición real con la regla y esperar que la envolvente se estabilice.
3. Registrar una tabla `posición_mm, nivel_adc` en varios puntos a lo largo del recorrido. Repetir ida y vuelta para observar histéresis, ruido y repetibilidad; medir sin dañar bobinas ni cables.
4. Graficar los datos, elegir un intervalo **monótono y sin ambigüedades** y verificar que exista una relación inversa suficientemente sensible. El sentido de crecimiento puede ser creciente o decreciente.
5. Implementar la conversión mediante interpolación entre puntos de la tabla. Limitar las consignas al tramo calibrado; marcar como inválida una lectura fuera de los límites o con poca sensibilidad.
6. Si aparecen niveles repetidos en dos posiciones útiles, no habilitar el lazo en todo ese intervalo. Opciones para conversar con el profesor: limitar el recorrido a una sola rama monotónica o cambiar la medición —por ejemplo, incorporar el segundo secundario o información de fase—. Esto último sería **un cambio de alcance**, no parte de la primera versión.

Propuesta simple para comenzar: calibración guardada en RAM y cargada al iniciar desde una tabla medida; persistencia en flash solo si el curso la requiere o queda tiempo. El sistema debe indicar **SIN_CALIBRAR** y no presentar centímetros como datos válidos si la calibración no existe o falla.

### 5.5 Servo y control de lazo cerrado

- El servo recibe un pulso PWM periódico; para un SG90 se puede comenzar con un período de **20 ms** y un intervalo aproximado de pulso de **1 a 2 ms**, sujeto al servo real. No asumir que alcanza un ángulo exacto ni forzarlo contra topes.
- Alimentar el servo con una fuente adecuada a su corriente pico, separada de la alimentación lógica; unir masas en forma adecuada y desacoplar. Confirmar que el servo comprado acepta el nivel lógico del pin de la LPC1769.
- El mecanismo de engranaje y cremallera es un prototipo. Comprobar dirección, recorrido, fuerza, sujeción del tubo, salida de cables y acople a ferrita antes de automatizar. El [paquete de impresión v2](hardware/prototipo_mecanico_v2.zip) contiene los STL y sus instrucciones; no prueba funcionamiento físico.
- El servo controla su propio eje, pero eso **no mide directamente** la posición real de la ferrita. El control externo usa el valor calibrado por ADC como realimentación.

Cada 20 ms, inicialmente:

```text
si no hay calibración válida, la medición está vieja o existe falla:
    inhibir movimiento automático y publicar el motivo
si hay un destino válido y una medición reciente:
    error_mm = destino_mm - posicion_medida_mm
    si |error_mm| está dentro de la tolerancia:
        mantener el último pulso seguro y marcar EN_POSICION
    si no:
        modificar el pulso del servo un paso pequeño en el sentido calibrado
        limitar pulso y recorrido a valores comprobados
        marcar MOVIENDO
```

El sentido de corrección se **calibra**: depende del montaje del engranaje y de cómo cambie la lectura con la posición. Tolerancia en mm, paso de PWM, número de mediciones estables para declarar llegada y tiempo máximo de movimiento quedan pendientes de ensayo; incorporar una banda muerta/histéresis para reducir oscilación. Si el error no disminuye dentro del tiempo permitido, si aparece saturación, una medición vieja o un tope, detener nuevas órdenes y avisar una falla. La acción eléctrica exacta ante falla —mantener pulso o deshabilitarlo— se decide y prueba según la mecánica: ambas tienen consecuencias.

**Arranque:** no mover automáticamente al energizar. El controlador inicia en `SIN_CALIBRAR` o `LISTO` tras validar calibración, posición y límites. Una consigna fuera de rango se rechaza; no se reemplaza silenciosamente. Si se pierde UART mientras se mueve, la política inicial es detener la generación de nuevas órdenes y conservar el último pulso hasta evaluación de seguridad y reconexión; el control de seguridad no depende de la web.

## 6. Reparto de periféricos e interrupciones

Propuesta inicial; confirmar pines, clocks, modos y compatibilidad con la placa y con el driver al integrar.

| Recurso | Dueño único | Responsabilidad | Evitar conflictos |
| --- | --- | --- | --- |
| `DAC` y su contador | `excitation` | Preparar LUT y temporizar salida. | Ningún otro módulo escribe `DACR` mientras funciona DMA. |
| Canal DMA 0 | `excitation` | Tabla → DAC, con repetición por LLI. | Canal y descriptores reservados en configuración central. |
| `Timer0` + un Match compatible | `sampling` / configuración central | Disparar ADC a 8 kHz, a verificar. | Un solo módulo configura clock, Match, flanco y reset. No reutilizarlo sin acuerdo. |
| `ADC` y canal analógico | `sampling` | Medir envolvente. | Un solo propietario de registros y conversiones. |
| Canal DMA 1 | `sampling` | ADC → buffers | DMA y CPU no consumen el mismo registro sin coordinación. |
| `PWM1` | `servo` | Pulso del servo. | Un único dueño del período y de su canal de PWM. |
| `SysTick` | `timebase` | Reloj de milisegundos para control y telemetría. | Un solo `SysTick_Handler`; tareas se ejecutan fuera de la ISR. |
| `UART0` o alternativa | `serial` | Comandos y telemetría. | Confirmar pines y si la placa ya ofrece puerto serie virtual. |

### Contrato compartido para handlers

**Existe un solo símbolo por interrupción**, independientemente de cuántas funciones la necesiten. No crear dos implementaciones de `DMA_IRQHandler`, `TIMER0_IRQHandler` o `SysTick_Handler` en archivos distintos. Propuesta:

```text
DMA_IRQHandler único:
    leer causas de fin y error para TODOS los canales asignados
    limpiar flags de cada causa atendida
    notificar a excitation o sampling según el canal
    salir; no promediar, imprimir por UART ni controlar el servo aquí

SysTick_Handler único:
    incrementar contador de tiempo / señalizar tareas periódicas
    salir

main / bucle principal:
    procesar buffers listos → actualizar medición
    procesar comandos → ejecutar control periódico → enviar telemetría
```

Si el Timer0 solo genera un Match de hardware para el ADC, puede que **no necesite ISR**. Si más adelante sí se usa, su dueño mantiene un único handler y despacha los eventos explícitamente. Declarar en un archivo de configuración quién usa cada canal DMA, timer, pin y prioridad NVIC. El init global de clocks, GPDMA y NVIC sucede **una sola vez** y antes de iniciar canales.

Entre ISR y `main`, `volatile` puede impedir optimizaciones de una bandera, pero **no resuelve por sí solo atomicidad, orden, ni propiedad del buffer**. Definir quién escribe, cuándo se cede un bloque, cómo se detecta overrun y qué ocurre si llegan dos bloques antes de procesarlos. Si se comparten estructuras de varias palabras, usar copia protegida o sección crítica corta según corresponda.

## 7. División del código y contratos

La estructura siguiente es una propuesta de implementación; **estos archivos y funciones todavía no existen** en el proyecto. Antes de programar, acordar nombres y contratos en headers compartidos para evitar dos implementaciones de un mismo periférico.

```text
.
├── README.md                    presentación según plantilla
├── ARCHITECTURE.md              este documento
├── docs/                        requisitos y evidencia
├── hardware/                    prototipo mecánico y futuro esquemático
├── firmware/                    estructura, implementación pendiente
│   ├── inc/                     headers y contratos
│   └── src/
│       ├── main.c               inicialización, bucle y estado general
│       ├── board_config.c       clocks, pines y asignación de recursos
│       ├── irq_dispatch.c       handlers únicos y despacho
│       ├── excitation.c        DAC, LUT, DMA de salida
│       ├── sampling.c           disparo ADC, DMA de entrada y buffers
│       ├── measurement.c        promedio y conversión a tensión
│       ├── calibration.c        tabla e interpolación a mm
│       ├── servo.c              PWM y límites de pulso
│       ├── control.c            estados y lazo de posición
│       └── serial.c             UART, parser y telemetría
├── web/                         a crear: HTML, CSS y JavaScript
└── pruebas/                     a crear: datos de calibración y registros
```

### 7.1 Contratos entre módulos

| Módulo | Entrada → salida | Responsabilidad y límites |
| --- | --- | --- |
| `board_config` | Inicialización única → configuración de placa | Define pines, clocks, recursos, canales DMA y dueños; informa errores. |
| `excitation` | `start()` → señal DAC | Genera 1 kHz de referencia. Conserva propiedad de LUT/LLI durante DMA. La tensión RMS en la bobina se calibra con instrumentos. |
| `sampling` | Trigger y ADC → bloque crudo + tiempo | Produce bloques estables; informa overrun y errores DMA; no calcula posición. |
| `measurement` | Bloque crudo → promedio en cuentas y tensión | Interpreta bits, filtra/promedia y agrega calidad y timestamp; no mueve el servo. |
| `calibration` | Nivel + tabla válida → posición en **mm** o error | Interpola solo dentro del rango monotónico; sin calibración no inventa posiciones. |
| `servo` | Pulso solicitado en **µs** → PWM limitado | No decide por sí solo la posición; aplica límites verificados y devuelve estado. |
| `control` | Consigna y medición en **mm** + tiempo → orden al servo | Único dueño del estado de lazo, de los límites y del sentido de corrección. |
| `serial` | Bytes UART ↔ comandos y telemetría | Valida tramas y entrega comandos a `control`; no escribe periféricos de otros módulos. |
| `web` | Usuario ↔ puerto serie | Convierte cm de pantalla a mm del protocolo; muestra datos, unidades, conexión y fallas. |

**Unidades:** firmware y protocolo usan milímetros para posiciones, milisegundos para tiempo y microsegundos para pulso PWM. La web puede mostrar e ingresar centímetros, pero convierte explícitamente: `1 cm = 10 mm`. Los valores crudos del ADC son **cuentas**, no voltios ni milímetros.

**Estado compartido mínimo:** `timestamp_ms`, `adc_promedio`, `tension_adc`, `posicion_mm` + bandera de validez, `consigna_mm`, `pulso_us` y estado del controlador. No leer datos parciales mientras otro contexto actualiza una estructura.

### 7.2 Plan de trabajo en paralelo

| Persona | Trabajo principal | Entrega que la otra persona puede usar |
| --- | --- | --- |
| Integrante A | Hardware analógico, excitación DAC/DMA, medición ADC/DMA y calibración. | `excitation`, `sampling`, `measurement`, `calibration` y lecturas con timestamp y validez. |
| Integrante B | Mecanismo y servo, lazo de control, UART y web. | `servo`, `control`, `serial`, interfaz web y protocolo probado con datos simulados. |
| Ambos | Acordar headers, pines, prioridades y responsables de IRQ; integración, medición y pruebas. | Configuración común, contratos estables y prueba de extremo a extremo. |

Cada integrante puede empezar con un sustituto: A prueba lecturas sin mover el servo; B usa una función ficticia que entrega posiciones conocidas. Los sustitutos deben anunciarse como **simulados**, nunca mezclarse con datos físicos. Antes de integrar, revisar los contratos juntos y probar un módulo por vez. La asignación A/B es intercambiable entre ustedes.

### 7.3 Inicio e integración

1. Inicializar reloj, pines y tiempo; definir GPDMA una sola vez y registrar responsables de canal y handlers.
2. Inicializar servo en estado inhibido, UART y módulos analógicos con salidas seguras.
3. Preparar tabla DAC, buffers ADC y descriptores DMA en memoria válida y alineada; habilitar las solicitudes y arrancar periféricos en el orden comprobado.
4. Esperar medición estable, comprobar calibración y establecer límites antes de aceptar `MOVER`.
5. En el bucle principal procesar buffers, comandos y tareas periódicas sin bloquear indefinidamente; supervisar errores, datos viejos y sobrecarga.

**Drivers existentes:** en el entorno local del curso hay guías de DAC/DMA y drivers LPC17xx, aún no incorporados a este repositorio. Una guía local advierte un posible problema en una versión de `GPDMA_ConfigDMAReqSel()` con conexiones 0–7 y memoria, y una configuración automática de ancho para DAC: **revisar el código concreto y corregir/verificar antes de reutilizarlo**. No copiar un ejemplo sin comprobar versión, anchos y descriptores.

## 8. Comunicación UART y página web

### 8.1 Conexión

Conectar UART de 3,3 V de la LPC1769 a un adaptador USB-serie compatible o al puerto serie virtual que tenga la placa; confirmar pines, niveles, alimentación y masa común. Propuesta inicial: **115200 baudios, 8N1**, sin control de flujo; ambos extremos deben coincidir. Evitar datos de 5 V en pines que no admitan ese nivel.

La web puede usar **Web Serial** desde un navegador compatible y un contexto seguro como `localhost` o HTTPS; el usuario debe seleccionar el puerto. Si el navegador disponible no lo soporta, alternativa: un pequeño programa local que abra el puerto y sirva la página. El navegador no controla directamente los periféricos: solo intercambia mensajes por el puerto serie.

### 8.2 Protocolo inicial

Texto ASCII con **una línea por mensaje**, terminada en `\n`; aceptar `\r\n`. Cada línea tiene longitud máxima acordada —por ejemplo, 96 bytes— y los comandos incompletos se acumulan sin bloquear el control. Comas como separador; punto decimal; posiciones en **mm**. Ejemplo propuesto:

```text
PC → placa
PING\n
ESTADO\n
MOVER,20.0\n
DETENER\n

placa → PC
OK,PING\n
OK,MOVER,20.0\n
ERR,SIN_CALIBRAR\n
ERR,FUERA_DE_RANGO\n
DATOS,12340,512,1.650,18.4,20.0,1520,MOVIENDO\n
```

`DATOS` contiene `tiempo_ms,adc_cuentas,voltaje_adc_V,posicion_mm,consigna_mm,pulso_us,estado`. `NA` indica valor no disponible, por ejemplo una posición sin calibrar. `OK,MOVER` significa **consigna aceptada**, no que ya llegó; la llegada se informa en un `DATOS` con estado `EN_POSICION`. Los números mostrados son ejemplos de formato, no una calibración real.

Definir y probar límites: longitud máxima, caracteres inválidos, número no finito, órdenes fuera de rango, comandos mientras se calibra, comunicación cortada y búfer lleno. Una trama inválida produce `ERR,...` sin mover el servo. `DETENER` cancela la consigna y aplica la política de pulso acordada; no reemplaza un corte físico de alimentación.

### 8.3 Qué mostrará la web

- Selección de puerto, conectar/desconectar y estado de la comunicación.
- Entrada de posición deseada en **cm**, botón de enviar y botón de detener.
- Posición estimada, consigna, error, voltaje acondicionado, estado y aviso de calibración/falla, con unidades claras.
- Gráfico en el tiempo de posición medida y consigna. Opcional: segundo gráfico de voltaje o indicador del ADC para depuración.
- Ilustración del tubo y la ferrita basada en la **posición medida**, identificada como representación; si no hay medición válida, no dibujar una posición ficticia como real.
- La web comienza con datos simulados para permitir trabajo paralelo, identificando ese modo. La versión conectada debe verificar que la respuesta corresponde al firmware real.

## 9. Verificación y criterios de aceptación

| Prueba | Cómo realizarla | Resultado esperado |
| --- | --- | --- |
| Excitación | Osciloscopio en el DAC y luego en el primario conectado. | Seno continua de 1 kHz; ajustar hasta 1 V RMS en la bobina sin recorte ni calentamiento anormal. |
| Secundario | Medir alterna en varias posiciones antes del detector. | Verificar amplitud, frecuencia, rango y que la forma cambia con la posición. |
| Detector | Observar entrada AC y salida al mover ida y vuelta. | Envolvente sube y baja, sin saturación ni rizado excesivo para la tasa ADC elegida. |
| ADC y DMA | Alimentar con niveles conocidos; medir tiempo y bloques. | Lectura coherente, tasa próxima a 8 kHz y ningún overrun no atendido. |
| Calibración | Registrar tabla al avanzar y retroceder; comprobar puntos intermedios. | Identificar intervalo monotónico, repetibilidad y error; validar antes de dar mm. |
| PWM y mecánica | Medir pulso; mover en pasos pequeños, inicialmente sin núcleo. | Movimiento suave, sin choques, sin saltar dientes y con carrera permitida. |
| UART | Enviar comandos válidos, inválidos y fuera de rango desde terminal. | Respuestas claras, sin movimiento ante órdenes rechazadas y sin bloquear medición. |
| Web | Conectar por puerto, enviar consigna, desconectar/reconectar. | Muestra estado y datos reales con unidades; refleja errores y desconexión. |
| Lazo completo | Solicitar puntos dentro del rango calibrado desde ambas direcciones. | Se aproxima y se mantiene dentro de la tolerancia acordada, sin toparse ni oscilar de forma sostenida. |
| Falla | Simular lectura inválida, desconexión, falta de calibración y demora excesiva. | No se inician nuevas órdenes automáticas y se informa el motivo. |

**Pendiente para aceptar el TP:** acordar con el docente precisión en mm, tiempo de establecimiento, rango útil, tolerancia de amplitud/frecuencia y comportamiento exacto ante fallas. No inventar números de precisión antes de medir el sensor.

## 10. Cronograma de cuatro semanas

| Semana | Objetivo | Resultado observable |
| --- | --- | --- |
| 1 | Acordar contratos/pines; caracterizar bobinas y generar DAC + DMA. En paralelo, preparar UART/web simulada y probar el servo por separado. | Señal de 1 kHz en el primario bajo carga; comunicación inicial y movimiento manual seguro. |
| 2 | Armar y medir el detector; activar ADC + DMA y recolectar datos de calibración. En paralelo, terminar soporte y adaptar el servo. | Tabla preliminar posición–nivel y lectura estable en PC. |
| 3 | Implementar calibración, estados, control y protocolo; integrar interfaz con datos reales. | El sistema acepta una consigna válida y se mueve con límites controlados. |
| 4 | Ajustar, medir error/repetibilidad y retardo; probar fallas, documentar y preparar demostración. | Recorrido útil y precisión medidos, explicación completa y prueba final. |

## 11. Preguntas pendientes para el profesor y las pruebas

1. ¿Qué rango útil y tolerancia de posición se esperan? ¿Se exige la carrera completa de 100 mm o puede determinarse luego de medir? Las longitudes del tubo y núcleo son aproximadas.
2. ¿El detector con diodo simple tiene suficiente sensibilidad en el secundario medido o hace falta rectificador de precisión / ganancia? ¿Qué frecuencia de respuesta se necesita?
3. ¿La curva de un solo secundario tiene una rama monotónica suficientemente amplia? ¿Se acepta limitar el recorrido a esa rama?
4. ¿Cómo se definirá el cero físico y cómo se validará que la unión a la ferrita permita empujar y tirar sin dañar el núcleo?
5. ¿Qué modelo exacto de SG90, rango de giro, fuente, brazo, tornillería y pines reales están disponibles? ¿El mecanismo alcanza el recorrido con margen?
6. ¿Se requiere guardar calibración en flash o es suficiente cargarla al iniciar? ¿Hay requisitos de muestreo o DMA más específicos?

## 12. Referencias

- [Hoja de datos LPC1769/68/67/66/65/64/63 — NXP](https://www.nxp.com/docs/en/data-sheet/LPC1769_68_67_66_65_64_63.pdf).
- [Muestreo, aliasing y filtrado — Texas Instruments](https://www.ti.com/lit/gpn/adc121s021): fundamento general de antialiasing; no es la ficha del ADC LPC1769.
- [Medición de posición y desplazamiento con LVDT — NI](https://www.ni.com/en/shop/data-acquisition/measuring-position-and-displacement-with-lvdts.html): referencia sobre un LVDT diferencial convencional y sus diferencias respecto de este montaje de un secundario.
- [Detector de picos y efecto de la caída del diodo — Analog Devices](https://wiki.analog.com/university/courses/electronics/text/chapter-7).
- [Web Serial API — MDN](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API).

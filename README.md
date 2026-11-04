# Medición y control de posición con sensor tipo LVDT

- **Asignatura:** Electrónica Digital III — Universidad Nacional de Córdoba.
- **Integrantes:** pendientes de completar.
- **Docente:** pendiente de completar.
- **Estado:** diseño y planificación; los resultados físicos todavía no están medidos.

## 1. Descripción general del proyecto

Se desarrollará un sistema didáctico de medición y control de posición lineal con una LPC1769. Un núcleo de ferrita se desplazará dentro de un tubo con bobinas; el microcontrolador excitará la bobina primaria, medirá la señal inducida en un secundario y estimará la posición mediante una calibración experimental.

Una computadora enviará la posición solicitada por UART desde una página web. El microcontrolador comparará esa consigna con la posición medida y accionará un servomotor para mover la ferrita. El proyecto está orientado a integrar sensor, adquisición, acondicionamiento analógico, actuador y lazo de control.

### Alcances del proyecto

- **Incluye:** una bobina primaria y un secundario, excitación DAC/DMA, detector de envolvente, ADC/DMA, calibración, control del servo por PWM, comunicación UART y una interfaz web.
- **Primera versión:** el segundo secundario no se utiliza. El intervalo de posición válido se determinará midiendo y eligiendo un tramo monotónico de la respuesta; no se presupone que 100 mm de carrera sean alcanzables.
- **Fuera del alcance inicial:** conexión inalámbrica, PCB final, medición diferencial con dos secundarios, precisión comercial garantizada y persistencia de calibración en flash sin requerimiento confirmado.

### Posibles etapas siguientes

- Ampliar el intervalo válido mediante una medición diferencial o sensible a fase, si el profesor lo aprueba.
- Diseñar un circuito impreso y mejorar la protección y el acondicionamiento.
- Guardar calibración y exportar registros de ensayos.

## 2. Arquitectura del sistema: hardware y software

### Hardware e interconexión

```text
DAC + DMA → adaptación / amplificación → primario
                         núcleo móvil → secundario → diodo + filtro → ADC + DMA
                                                                  ↓
Computadora ↔ USB-UART ↔ LPC1769 → control → PWM → servo → mecanismo
```

La excitación objetivo en el primario es una señal alterna de **1 kHz y 1 V RMS**. La propuesta de partida genera **0,5 V RMS de componente alterna en el DAC** y utiliza una etapa externa de ganancia cercana a 2, que debe dimensionarse según la bobina y comprobarse con carga. El secundario se rectifica y filtra antes del ADC; su tensión y la entrada permitida del ADC requieren medición y adaptación.

El [paquete mecánico v2](hardware/prototipo_mecanico_v2.zip) contiene un soporte y un accionamiento imprimibles **provisionales**. La unión a la ferrita, el diámetro interior del tubo y el recorrido del servo aún no están verificados. El esquemático eléctrico y la lista definitiva de componentes quedan pendientes.

### Arquitectura del firmware

Se propone firmware *bare metal* con tareas cooperativas y handlers únicos por interrupción. Los módulos propuestos se ocupan de configuración, excitación DAC, adquisición ADC, promedio, calibración, servo, control y UART. La web usa el protocolo serie para mostrar posición y consigna; el lazo corre en la placa.

La [arquitectura técnica detallada](ARCHITECTURE.md) incluye contratos, propiedad de timers/IRQ/DMA, protocolo, división entre integrantes, calibración y pruebas.

## 3. Especificaciones eléctricas, alimentación y entorno

| Parámetro | Propuesta o estado |
| --- | --- |
| Controlador | LPC1769, Cortex-M3. |
| Lógica y referencias analógicas | Nominalmente 3,3 V; verificar la placa y sus referencias. |
| Excitación | 1 kHz y 1 V RMS de alterna **en el primario con carga**. |
| ADC de envolvente | 8.000 muestras/s y bloques de 64, como punto de partida. |
| Servo | SG90 con fuente adecuada a su corriente pico; masa común y compatibilidad con señal de 3,3 V por verificar. |
| Consumo y tolerancias | Pendientes de medir en el equipo real. |
| Ambiente | Uso de laboratorio; condiciones definitivas no especificadas. |

### Electrónica Digital III: plataforma y concurrencia

- **IDE, SDK, compilador y versiones:** pendientes de confirmar y documentar.
- **Programador/depurador:** pendiente de confirmar según la placa.
- **Periféricos previstos:** ADC, DAC, GPDMA, Timer Match, PWM1, UART, SysTick y NVIC.
- **Concurrencia:** bucle principal cooperativo, ISR breves y un dueño por periférico y handler; sin RTOS previsto.
- **Pines y relojes:** propuesta en [ARCHITECTURE.md](ARCHITECTURE.md); completar y validar con el esquemático de la placa antes de conectar.

## 4. Proceso de integración y desarrollo

1. **Base:** acordar contratos, configuración de placa, recursos e interrupciones; compilar un programa mínimo.
2. **Generación y adquisición:** probar DAC/DMA, el amplificador del primario, el detector y ADC/DMA con instrumentos.
3. **Módulos en paralelo:** calibración y medición por un lado; servo, UART, control y web por el otro, usando datos simulados cuando corresponda.
4. **Integración:** unir el mecanismo, calibrar la relación entre voltaje y posición, habilitar el lazo dentro del tramo válido y comprobar fallas.

La distribución propuesta, el cronograma y los criterios de cada etapa están en [ARCHITECTURE.md](ARCHITECTURE.md).

## 5. Ensayos, pruebas y resultados

**Pendientes de realización.** Se registrarán mediciones de frecuencia y RMS en el primario bajo carga, la envolvente del secundario, la tasa de ADC y el DMA, la curva de calibración, las señales PWM/UART y la respuesta completa del lazo. Deben adjuntarse capturas de instrumental, gráficos y fotografías del prototipo real. Las simulaciones geométricas no se presentan como mediciones físicas.

Los criterios propuestos y las decisiones aún abiertas están en [ARCHITECTURE.md](ARCHITECTURE.md#9-verificación-y-criterios-de-aceptación).

## 6. Estructura del repositorio

```text
├── README.md             # Presentación según la plantilla de la materia
├── ARCHITECTURE.md       # Diseño completo, contratos, protocolo y pruebas
├── firmware/
│   ├── src/              # Código C: pendiente
│   └── inc/              # Headers: pendientes
├── hardware/             # Prototipo mecánico y futuro esquemático/BOM
├── docs/                 # Requisitos y futura evidencia
└── web/                  # Interfaz: pendiente
```

La estructura sigue la [plantilla de la materia](https://github.com/MarcosUNC/TPfinalDIGII-III). Los módulos de firmware y la web aún no están implementados; sus nombres describen el trabajo planificado.

# Plan de issues para el proyecto LVDT

Estas tareas sirven para trabajar en paralelo. **Son propuestas hasta que aparezcan como issues reales en GitHub**; no implican que el firmware o el hardware ya estén implementados. La asignación entre integrantes se acuerda al comenzar cada tarea.

## 1. Proyecto base y compilación para LPC1769

Crear la estructura de compilación del firmware, definir SDK/compilador, linker y un `main` mínimo. Documentar el procedimiento de compilación desde cero.

**Aceptación:** compila de forma reproducible; se registran herramienta, versión y placa; los directorios generados no se suben al repositorio.

## 2. Contratos compartidos, pines y propiedad de interrupciones

Definir headers para las interfaces de los módulos, unidades, errores, estados, pines, clocks y responsables de PWM, timers y canales DMA. Unificar el despacho de handlers.

**Aceptación:** no hay dos implementaciones del mismo vector ni dos dueños de un periférico; los contratos identifican en qué contexto se ejecutan, la propiedad de los buffers y el manejo de errores.

## 3. Excitación senoidal: DAC y DMA

Generar una senoidal de 1 kHz con 32 muestras/ciclo y 32 kactualizaciones/s, usando LUT, contador DAC y DMA circular. Partir de la tabla de 0,5 V RMS de alterna, centrada en la referencia; el objetivo de 1 V RMS es en el primario tras la etapa externa.

**Aceptación:** frecuencia y forma medidas con osciloscopio, sin discontinuidades observables; se comprueban el formato de `DACR`, el bit `BIAS`, relojes y el manejo de DMA.

## 4. Etapa de potencia y excitación del primario

Medir la bobina y dimensionar el adaptador/amplificador y el acoplamiento de continua para alcanzar 1 V RMS en el primario conectado. Dibujar esquema y documentar alimentación, protecciones y componentes.

**Aceptación:** 1 kHz y 1 V RMS medidos entre los terminales de la bobina bajo carga; señal sin recorte y circuito compatible con corriente, referencia y fuente reales.

## 5. Detector y acondicionamiento del secundario

Caracterizar un solo secundario e implementar diodo, descarga RC, buffer/ganancia y protección para el ADC. Ajustar el filtro observando subida, bajada, rizado y ruido.

**Aceptación:** salida dentro de los límites reales del ADC, sin saturación, con respuesta y rizado medidos; valores y esquema documentados.

## 6. Muestreo ADC por timer y DMA

Implementar un canal de adquisición de la envolvente a 8.000 muestras/s como punto de partida, con bloques de 64, doble buffer y monitoreo de overrun. Verificar que la ruta timer→ADC→DMA funcione con la placa y el driver reales.

**Aceptación:** tasas y bloques medidos, datos consistentes con tensiones conocidas, sin pérdida no detectada; manejo de errores y bit/registro usados documentados.

## 7. Procesamiento y calibración posición–nivel

Promediar bloques, convertir cuentas a tensión y registrar la curva del único secundario en ida y vuelta. Delimitar una región monotónica; implementar tabla e interpolación a mm con validez y marcas de tiempo.

**Aceptación:** tabla y gráfico obtenidos de mediciones, puntos intermedios comprobados y comportamiento seguro para datos ambiguos, obsoletos o sin calibrar.

## 8. Mecanismo y sujeción de la ferrita

Validar físicamente el prototipo mecánico, el diámetro interior del tubo, las bobinas y la unión desmontable o adecuada a la ferrita. Medir carrera, topes, esfuerzos y ajuste de la guía.

**Aceptación:** se puede empujar y tirar sin dañar el núcleo, el tubo permanece sujeto y quedan documentados el recorrido seguro y los cambios mecánicos necesarios.

## 9. Servo PWM y límites

Implementar el módulo PWM del SG90 con pulso configurable en µs, límites, habilitación y estado. Verificar alimentación, corriente pico, nivel lógico y brazo del servo comprado.

**Aceptación:** período y pulsos medidos; el servo responde sin llegar a topes y sin caídas de alimentación; los límites reales quedan documentados.

## 10. Control de posición y fallas

Crear la máquina de estados y el lazo: consigna y posición en mm, corrección acotada, sentido calibrado, zona muerta, límites y tiempos máximos. Mantener deshabilitado el automático sin datos válidos.

**Aceptación:** con medición simulada y luego real, llega al rango calibrado sin movimiento ante datos inválidos, detecta falta de avance y reporta el estado; las tolerancias medidas no se inventan.

## 11. UART y protocolo

Implementar recepción no bloqueante de líneas y telemetría, con límites de longitud y parser. Acordar `PING`, `ESTADO`, `MOVER` y `DETENER`, unidades mm/µs/ms y errores según la arquitectura.

**Aceptación:** comandos válidos e inválidos tienen respuesta coherente, no hay desbordes ni bloqueos del control y se prueba la desconexión del puerto.

## 12. Interfaz web y visualización

Crear la interfaz en `web/` con modo simulado explícito, Web Serial, consigna en cm, detener, estados, gráficos de posición/consigna y representación del núcleo basada en datos reales.

**Aceptación:** se puede trabajar sin placa en modo simulado; con placa conecta por puerto, convierte cm a mm, muestra datos válidos y señala fallas/desconexión. No presenta datos simulados como reales.

## 13. Integración, ensayos y entrega

Integrar analógica, firmware, mecanismo y web; calibrar, ensayar puntos de ida y vuelta, tiempos de establecimiento, ruido, errores y fallas; completar el README con evidencia de osciloscopio, gráficos y fotos.

**Aceptación:** una demostración reproducible recorre el tramo real calibrado y cumple los criterios acordados con el docente; la documentación separa simulación, diseño propuesto y mediciones reales.

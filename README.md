# TP final de Electrónica Digital III
Girasol electrónico: seguidor de luz con LPC1769

###  Idea y objetivo

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

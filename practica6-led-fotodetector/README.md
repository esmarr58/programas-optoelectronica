# Práctica 6 — El LED como fotodetector (TIA con LM358 a 5 V)

**Optoelectrónica (V3736) · CUCEI–UDG · 2026B · Dr. Rubén Estrada Marmolejo**

Antesala del fotodiodo: un LED **rojo, verde o azul** hace de **detector** en un amplificador
de transimpedancia (TIA) con **LM358 a una sola fuente de 5 V**, y otro LED (**rojo, verde,
azul o blanco**) lo ilumina. Se miden las 12 combinaciones detector × emisor, en modo
**fotovoltaico** (FV) y **fotoconductivo** (FC), y se explica con la energía del fotón por qué
cada detector solo «ve» su color y los de mayor energía.

## Archivos

| Archivo | Contenido |
|---|---|
| `practica6_led_detector/practica6_led_detector.ino` | Programa (Arduino-ESP32 3.x, placa **ESP32S3 Dev Module**) |
| `diagrama-conexiones.pdf` / `.tex` | Esquema: emisor con NPN, pedestal V_ref, TIA con J1 y divisor al ADC |
| `diagrama-bloques.pdf` / `.tex` | Bloques del sistema y del comando `medir` |
| `guia-practica6/` | Guía guiada (DOCX editable + PDF): se llena y se entrega como un solo PDF |

## Conexiones

| Elemento | Conexión |
|---|---|
| LM358 pin 8 / pin 4 | Pin **5V** de la ESP32-S3 / GND; 100 nF de pin 8 a GND |
| Pedestal (mitad B) | +5 V → 39 kΩ → pin 5 → 10 kΩ → GND; 1 µF de pin 5 a GND; pin 6 unido a pin 7. **Pin 7 = V_ref ≈ 1.02 V** |
| TIA (mitad A) | Pin 3 a V_ref. Pin 2: cátodo del LED detector, R_f y C_f. Pin 1 = V_o |
| R_f ∥ C_f | 10 MΩ ∥ 1 nF. Si satura: 1 MΩ y luego 100 kΩ. Si el oscuro queda abajo de 0.3 V: 1 MΩ |
| Ánodo del detector → J1 | **FV:** a V_ref (0 V en el LED). **FC:** a GND (≈1 V en inversa) |
| Lectura | Pin 1 → 10 kΩ → **GPIO4** → 22 kΩ → GND. Pin 7 (V_ref) → 10 kΩ → **GPIO6** |
| LED emisor | +5 V → 220 Ω → LED → colector NPN; emisor a GND (cable aparte al GND de la ESP32); base por 4.7 kΩ desde **GPIO5** |

Emisor y detector van nariz con nariz dentro de un tubo negro de ≈5 mm, fuera de la
protoboard, con cables dupont cortos (≤ 5 cm), siempre al mismo tope. Cambia de LED con el
emisor apagado (`led off`). Control ciego: un disco de cartulina negra en el tubo; `medir`
debe dar ≈ 0.

## Por qué el pedestal

Con una sola fuente, la salida del LM358 no puede bajar de 0 V. El nodo del TIA se fija en
V_ref ≈ 1 V y la salida queda en `V_o = V_ref + (I_ph − I_B)·R_f`. La entrada PNP del LM358
entrega su corriente de polarización I_B (≈20 nA típica) al nodo, por eso en oscuridad
V_o queda algo abajo de V_ref. `medir` resta el oscuro tomado justo antes, y así se cancelan
V_ref, I_B y el offset; también resta el cambio de V_ref entre oscuro y luz (el emisor carga
el riel de 5 V).

## Uso (Monitor Serial a 115200 baudios, «Nueva línea»)

| Comando | Qué hace |
|---|---|
| `det rojo` · `det verde` · `det azul` | LED que pusiste como detector (también `ro`, `ve`, `az`) |
| `emi rojo` · `emi verde` · `emi azul` · `emi blanco` | LED que pusiste como emisor (también `bl`) |
| `modo fv` / `modo fc` | Posición de J1 |
| `rf 10000000` / `rf 1000000` | R_f colocada, en ohm |
| `medir` | Oscuro → luz → resta, `rep` veces (3); guarda el promedio en la matriz |
| `tabla` / `borrar` | Imprime / borra las matrices 3 × 4 (FV y FC); `*` marca casillas dudosas |
| `leer` / `stream on|off` | V_o y V_ref ahora / cada 0.5 s |
| `led on` / `led off` | Emisor encendido o apagado a mano |
| `csv` | Encabezado del CSV (escríbelo una vez al empezar) |
| `rep N` · `prom N` · `espera N` | Repeticiones, muestras por lectura, ms de espera |

CSV: `tiempo_ms,modo,rf_ohm,detector,emisor,rep,Vref_mV,Vo_osc_mV,Vo_luz_mV,Vo_neto_mV,I_nA,estado`

- `Vo_*` es el voltaje en la salida del LM358 (GPIO4 × 1.4545).
- `Vo_neto = (Vo_luz − Vref_luz) − (Vo_osc − Vref_osc)`; `I_nA = Vo_neto_mV × 10⁶ / rf_ohm`.
- `estado = SATURA` si V_o,luz > 3.1 V (baja R_f); `PISO` si V_o,osc < 0.15 V (usa 1 MΩ);
  `INVERTIDO?` si la luz baja la salida (detector al revés).

El programa no cambia el modo, R_f ni los LED: solo los etiqueta. Si olvidas `det`, `emi` o
`modo`, el dato se guarda en la casilla equivocada de la matriz.

## Seguridad

- Solo 5 V: no alimentes el LM358 con 12 V (su salida dañaría la ESP32).
- La salida del LM358 entra a la ESP32 únicamente por el divisor 10 kΩ / 22 kΩ.
- No mires de frente el emisor encendido a corta distancia.

## Verificación

Compilado con Arduino CLI 1.5.1 y Arduino-ESP32 3.3.12 (`esp32:esp32:esp32s3`), sin
advertencias: 319 010 bytes de programa y 22 784 bytes de variables globales. El montaje y
los niveles de señal se validan en el laboratorio.

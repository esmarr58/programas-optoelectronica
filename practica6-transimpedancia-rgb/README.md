# Práctica 6 — Amplificador de transimpedancia con BPW34 y LED rojo, verde y azul

**Optoelectrónica (V3736) · CUCEI–UDG · 2026B · Dr. Rubén Estrada Marmolejo**

Un solo circuito: el fotodiodo **BPW34** entra a un amplificador de transimpedancia (TIA) con
**TL072** a ±12 V. Tres LED (rojo, verde y azul) iluminan el fotodiodo, uno a la vez, desde la
misma posición. Con el puente **J1** el fotodiodo trabaja en modo **fotovoltaico** (ánodo a GND,
0 V en el diodo) o **fotoconductivo** (ánodo a −12 V por 10 kΩ, ≈12 V de polarización inversa).
La ESP32-S3 enciende cada color, lee la salida del TIA por el ADC (protegido) y calcula la
fotocorriente `I_ph = (V − V_oscuro) / R_f`. Es un paso del espectrómetro del proyecto final:
el espectrómetro hará lo mismo, pero el color lo elegirá el ángulo del DVD.

## Archivos

| Archivo | Contenido |
|---|---|
| `practica6_tia_rgb/practica6_tia_rgb.ino` | Programa (Arduino-ESP32 3.x, placa **ESP32S3 Dev Module**) |
| `diagrama-conexiones.pdf` / `.tex` | Esquema completo: excitadores de LED, TIA con J1, protección del ADC |
| `diagrama-bloques.pdf` / `.tex` | Bloques del sistema y del programa (`secuencia`) |
| `guia-practica6/` | Guía guiada (DOCX editable + PDF): se llena y se entrega como un solo PDF |

## Conexiones

| Elemento | Conexión |
|---|---|
| Fuente dual | Dos canales en serie: +12 V, GND (punto medio), −12 V. GND = GND de la ESP32 |
| TL072 pin 8 / pin 4 | +12 V / −12 V, cada uno con 100 nF a GND junto al chip |
| TL072 pin 3 (IN+ A) | GND |
| TL072 pin 2 (IN− A) | Cátodo del BPW34, un extremo de R_f y de C_f |
| TL072 pin 1 (OUT A) | Otro extremo de R_f y C_f; punto de prueba V_o; R_S 10 kΩ hacia el ADC |
| R_f ∥ C_f | 100 kΩ ∥ 10 pF con los LED; 10 MΩ ∥ 10 pF para la prueba en oscuridad |
| Mitad B (pines 5, 6, 7) | Pin 5 a GND; pin 6 unido a pin 7 (seguidor, sin uso) |
| Ánodo del BPW34 → J1 | **FV:** a GND. **FC:** al nodo V_B = −12 V por R_B 10 kΩ, con C_B 100 nF a GND |
| Nodo ADC | R_S 10 kΩ desde V_o; Schottky D1 (ánodo en el nodo, cátodo a 3V3); Schottky D2 (ánodo a GND, cátodo en el nodo); C_S 100 nF a GND; nodo a **GPIO4** |
| LED rojo / verde / azul | +12 V → 1 kΩ → LED → colector NPN; emisor a GND |
| Bases NPN | GPIO5 (rojo), GPIO6 (verde), GPIO7 (azul), cada una por 4.7 kΩ |

D1 y D2: de preferencia BAT85 o BAT43 (fuga ≤ 2 µA). El 1N5819 de la P4/P5 sirve, pero fuga más
(µA): compara el ADC contra el multímetro cerca de 2.5 V y anota la diferencia. NPN: 2N2222A o
BC547 (verifica E-B-C en su hoja de datos). Identifica ánodo y cátodo del BPW34 con la prueba de
diodo del multímetro (punta roja en el ánodo: 0.4–0.7 V).

## Seguridad

- **La salida del TL072 nunca va directa a la ESP32**: puede llegar a ±10 V.
- Orden de encendido: USB de la ESP32 primero, después ±12 V. Al terminar: ±12 V fuera primero.
- Mide +12 V y −12 V **antes** de insertar el TL072; una polaridad invertida lo destruye.
- Retira los puentes de los bornes de la fuente a tierra/chasis: la masa del osciloscopio va a
  tierra física y, con un puente puesto, cortocircuitaría un canal.
- Cambia J1 y R_f con la fuente apagada.
- No mires de frente los LED a 10 mA a corta distancia.

## Uso (Monitor Serial a 115200 baudios, «Nueva línea»)

| Comando | Qué hace |
|---|---|
| `ayuda` / `estado` | Lista de comandos / parámetros y oscuro guardado |
| `led r` · `led g` · `led b` · `led off` | Enciende un color (g = verde) o apaga |
| `leer` | Una lectura promediada con el LED actual |
| `csv` | Imprime el encabezado del CSV (`leer` y `oscuro` no lo repiten) |
| `oscuro` | Apaga los LED, mide y guarda la referencia |
| `secuencia` | Oscuro, rojo, verde y azul en CSV |
| `stream on` / `stream off` | Lectura cada 0.5 s (para alinear y elegir la distancia) |
| `modo fv` / `modo fc` | Etiqueta el modo que armaste con J1 (borra el oscuro guardado) |
| `rf 100000` / `rf 10000000` | Informa la R_f colocada, en ohm (borra el oscuro guardado) |
| `prom N` / `espera N` | Muestras por lectura (1–1024) / ms tras cambiar de LED (50–5000) |
| `pulso b 1000` / `pulso off` | Onda cuadrada (5–20000 Hz) en un LED para el osciloscopio / detenerla |

CSV: `tiempo_ms,modo,rf_ohm,led,mV,mV_osc,mV_neto,I_uA,estado`

- `mV` es el voltaje **en el pin GPIO4**, promedio de `prom` lecturas calibradas.
- `I_uA = mV_neto × 1000 / rf_ohm`. Ejemplo: 1250 mV netos con 100 kΩ → 12.5 µA.
- `estado = SATURA` arriba de 2900 mV (acerca menos el LED o baja R_f);
  `BAJO` abajo de 50 mV (el ADC ya no es confiable: usa el multímetro en V_o).

El programa no cambia el modo ni R_f: los comandos `modo` y `rf` solo etiquetan el CSV y
corrigen el cálculo de la corriente. La lectura de referencia es el multímetro en V_o; el ADC
sirve para automatizar, igual que en el espectrómetro.

## Verificación

Compilado con Arduino CLI 1.5.1 y Arduino-ESP32 3.3.12 (`esp32:esp32:esp32s3`), sin
advertencias: 325 761 bytes de programa y 22 656 bytes de variables globales. El montaje y
los valores de señal se validan en el laboratorio.

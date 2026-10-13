# Práctica 4 — Convertidores de nivel y motor PWM con PC817 (ESP32-S3 · Arduino)

Programa y material de la **Práctica 4** del curso **Optoelectrónica (V3736)** —
Dr. Rubén Estrada Marmolejo, CUCEI–UDG. Tres circuitos aislados con el optoacoplador **PC817**:

| Circuito | Qué hace | Pin ESP32-S3 | Comando |
|---|---|---|---|
| **1** · 3.3 V → 24 V | La ESP32 conmuta una línea de 24 V | `GPIO4` (salida) | `c1 on` / `c1 off` |
| **2** · 24 V → 3.3 V | La ESP32 lee una señal de 24 V | `GPIO5` (entrada) | `c2` |
| **3** · motor PWM | El PC817 aísla el PWM; un MOSFET mueve el motor TT de 5 V | `GPIO6` (PWM 1 kHz) | `vel <0..100>`, `stop`, `rampa` |

`estado` imprime los tres circuitos y `ayuda` reimprime la lista de comandos.

## Qué hay en esta carpeta

- [`practica4_convertidores_pc817/`](practica4_convertidores_pc817/) — el programa (`.ino`), comentado.
- [`diagrama-conexiones.pdf`](diagrama-conexiones.pdf) — qué pata va a dónde en los tres circuitos.
- [`diagrama-bloques.pdf`](diagrama-bloques.pdf) — cómo está organizado el programa.
- [`guia-practica4/`](guia-practica4/) — la **guía guiada** (Word + PDF). **Se llena y se entrega:**
  no hay reporte aparte; el entregable es la guía exportada a **un solo PDF** (lunes 19 de octubre).

## Cómo subirlo (Arduino IDE)

1. Core **Arduino-ESP32 v3.x** (usa la API `ledcAttach`). Verificado con esp32 3.3.12.
2. Placa **«ESP32S3 Dev Module»**.
3. Abre `practica4_convertidores_pc817/practica4_convertidores_pc817.ino` y súbelo.
4. Monitor Serial a **115200 bps** con **«Nueva línea»**; escribe `ayuda`.

## ¿No tienes fuente de 24 V?

Usa una de **12 V con las mismas resistencias**: los tres circuitos funcionan. El Circuito 1 satura con más margen (necesita 1.18 mA de 4.75 mA disponibles) y en el Circuito 2 el LED recibe ≈ 2.3 mA (la salida necesita 0.31 mA de ≈ 1.15 mA disponibles). Anota en la guía qué fuente usaste.

## MOSFET del Circuito 3: IRLZ44N (canal N, nivel lógico)

Visto de frente (letras hacia ti, patas abajo): **1 = G** (compuerta) → colector del PC817 y R_C de 10 kΩ a +5 V · **2 = D** (drenador, también la pestaña) → terminal (−) del motor y ánodo del flyback · **3 = S** (fuente) → GND del motor. Un IRFZ44N o IRF540 (sin «L») necesita ~10 V en la compuerta y no sirve con 5 V.

## Tres cosas que no se te pueden olvidar

- **Las masas nunca se unen.** GND de la ESP32 por un lado; GND de 24 V y GND del motor por otro.
  Si las unes, todo sigue funcionando, pero pierdes el aislamiento que protege a la tarjeta.
- **Lógica invertida.** Con pull-up al colector, LED encendido → salida en BAJO. El programa la corrige
  en el Circuito 2 (`leerConvertidor2`) y en el 3 (`PWM_MAX - duty`).
- **El PC817 es lento** (t_r + t_f ≈ 7 µs): el PWM del motor va a **1 kHz**, no a 20 kHz.

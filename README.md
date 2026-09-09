# Programas de Optoelectrónica — Contador óptico 0–9 (ESP32-S3 · Arduino)

Programas de apoyo para la **Práctica 2** (contador óptico con switch de
fototransistor) del curso **Optoelectrónica (V3736)** — Dr. Rubén Estrada
Marmolejo, CUCEI–UDG.

Cada vez que un objeto **cruza el haz** entre el LED IR emisor y el
fototransistor **WP7113P3C**, se incrementa una cuenta **0 → 9 → 0** que se
muestra en un **display de 7 segmentos de cátodo común**.

## Los dos programas

| Carpeta | Modo del transistor | Cómo decide 0/1 | Ejercicio del deck |
|---|---|---|---|
| [`contador-corte-saturacion/`](contador-corte-saturacion/) | Corte y **saturación** | El **hardware**: `digitalRead()` (señal ya cuadrada) | Ejercicio 1 y diseño de R_L modo interruptor |
| [`contador-region-activa/`](contador-region-activa/) | **Región activa** | El **software**: `analogRead()` + umbral con histéresis | Ejercicio 3 (R_L en región activa) |

- **Corte/saturación:** con luz suficiente el transistor satura (V_CE ≈ 0.2–0.8 V,
  LOW) y sin luz entra en corte (V_CE ≈ 3.3 V, HIGH). Basta leer un pin digital.
- **Región activa:** el transistor **no** satura; V_CE = V_CC − I_C·R_L varía de
  forma continua con la luz. Se lee con el ADC y **tú** fijas el umbral en el
  código (puedes verlo y calibrarlo en el Monitor Serie).

## ⚠️ Voltaje de trabajo: 3.3 V

Alimenta el switch óptico a **3.3 V, NO a 5 V**. Así el colector del
fototransistor se conecta **directo** a un GPIO de la ESP32-S3 (máx. 3.3 V) sin
nivelador de nivel. Con 5 V dañarías el pin/ADC.

## Cableado

**Fototransistor (emisor común):**

```
   3V3 ──[ R_L 2.2 kΩ ]──┬── colector (C)         ── GPIO8  (señal)
                         │
                    (fototransistor WP7113P3C)
                         │
   GND ──────────────────┴── emisor (E)
```

- Haz **presente** → satura → colector ≈ 0 V (LOW / ADC bajo).
- Haz **interrumpido** → corte → colector ≈ 3.3 V (HIGH / ADC alto).

**Display de 7 segmentos (cátodo común):**

- Cada segmento **a…g** va de su GPIO **a través de una resistencia de
  220–330 Ω** al pin del segmento.
- El/los pin(es) **común** van a **GND**.
- (El punto decimal DP no se usa para contar 0–9.)

## Tabla de pines (ESP32-S3)

| Señal | GPIO |
|---|---|
| Segmento a | 4 |
| Segmento b | 5 |
| Segmento c | 6 |
| Segmento d | 7 |
| Segmento e | 15 |
| Segmento f | 16 |
| Segmento g | 17 |
| Sensor (colector) | 8 (ADC1) |

> GPIO8 es un canal de **ADC1**, así sirve para `digitalRead()` (programa
> digital) y para `analogRead()` (programa de región activa) sin recablear.

## Cómo subirlo (Arduino IDE)

1. Instala el soporte **esp32 by Espressif** (Boards Manager).
2. Placa: **ESP32S3 Dev Module** (o tu placa S3). USB CDC On Boot: *Enabled*.
3. Abre la carpeta del programa que quieras (`contador-corte-saturacion` o
   `contador-region-activa`) y sube el `.ino`.
4. Monitor Serie a **115200** baud.

## Calibración del modo analógico

1. Sube `contador-region-activa` y abre el Monitor Serie.
2. Anota el `ADC` con el **haz presente** (valor bajo) y con el **haz
   interrumpido** (valor alto).
3. Ajusta en el código `UMBRAL_BAJO` (un poco arriba del "presente") y
   `UMBRAL_ALTO` (un poco abajo del "interrumpido"). La histéresis entre ambos
   evita conteos falsos cuando la señal tiembla cerca del umbral.

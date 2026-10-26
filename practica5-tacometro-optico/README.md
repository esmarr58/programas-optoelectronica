# Práctica 5 — Tacómetro óptico con TCRT5000 e interrupciones (ESP32-S3 · Arduino)

Programa y material de la **Práctica 5** del curso **Optoelectrónica (V3736)** —
Dr. Rubén Estrada Marmolejo, CUCEI–UDG. Se imparte en la **Semana 11 (lunes 26 de octubre de 2026)**.

Reutiliza el **circuito 3 de la Práctica 4** (PC817 + MOSFET IRLZ44N + motor TT de 5 V) y le agrega un
sensor reflectivo **TCRT5000** que lee un **disco de 4 PPR** pegado al eje del motor. El colector del
fototransistor, con pull-up de 10 kΩ y un solo condensador de 10 nF, entra **directo a GPIO4** y cada
flanco **FALLING** se cuenta en una **interrupción externa** (`attachInterrupt`). Un filtro por software
de **tiempo muerto** (500 µs, configurable con `filtro N`) descarta rebotes; GPIO5 conmuta por cada pulso
aceptado para verlo en el osciloscopio.

| Señal | Pin ESP32-S3 | Para qué |
|---|---|---|
| PWM del motor (circuito 3 de P4) | `GPIO6` (salida, 1 kHz) | `vel 0..100`, `stop`, `rampa` |
| Colector del TCRT5000 (nodo A) | `GPIO4` (entrada, IRQ FALLING) | cuenta de pulsos → RPM |
| Mismo nodo A, diagnóstico | `GPIO10` (ADC1_CH9, 10 Hz) | `leer`: mV del colector, no cuenta RPM |
| Marcador para el osciloscopio | `GPIO5` (salida) | conmuta por cada pulso **aceptado** (CH2) |

## Qué hay en esta carpeta

- [`practica5_tacometro_optico/`](practica5_tacometro_optico/) — el programa (`.ino` + `tacometro.h`), comentado.
- [`diagrama-conexiones-v3.pdf`](diagrama-conexiones-v3.pdf) — qué pata va a dónde (sensor, filtro RC, motor aislado).
- [`diagrama-bloques-v3.pdf`](diagrama-bloques-v3.pdf) — cómo está organizado el programa (ISR, ventana de RPM, parser).
- [`Disco-Encoder-4PPR.pdf`](Disco-Encoder-4PPR.pdf) — plantilla del disco de 4 PPR; **imprimir al 100 %**.
- [`guia-practica5/`](guia-practica5/) — la **guía guiada** (Word + PDF). **Se llena y se entrega:**
  no hay reporte aparte; el entregable es la guía exportada a **un solo PDF**, con las **cuatro capturas
  obligatorias de osciloscopio** (condensador ausente/presente × filtro 0/500 µs). La fecha la confirma el profesor.
- [`tests/`](tests/) — pruebas del sketch en PC (API Arduino simulada); no hacen falta para la práctica.

## Cómo subirlo (Arduino IDE)

1. Core **Arduino-ESP32 v3.x** (usa `ledcAttach` y `ARDUINO_ISR_ATTR`). Verificado con esp32 3.3.12.
2. Placa **«ESP32S3 Dev Module»**.
3. Abre `practica5_tacometro_optico/practica5_tacometro_optico.ino` (con `tacometro.h` en la misma carpeta) y súbelo
   **sin la fuente del motor conectada**.
4. Monitor Serial a **115200 bps** con **«Nueva línea»**; escribe `ayuda`.

## Tres cosas que no se te pueden olvidar

- **Las masas nunca se unen.** GND de la ESP32 por un lado; GND del motor por otro. Las dos sondas del
  osciloscopio van a GND_ESP.
- **Lógica del sensor.** Sobre blanco el fototransistor conduce y el colector baja; sobre negro sube.
  Por eso el flanco que se cuenta es **FALLING** (negro → blanco).
- **El tiempo muerto no es histéresis.** `filtro 500` solo descarta flancos a menos de 500 µs del último
  aceptado; el primer pico espurio puede pasar. Por eso la evidencia compara las cuatro condiciones.

---

# Detalle técnico del programa

## Conexiones

Hacer el montaje sin fuente de motor. **GND_ESP y GND_motor permanecen separados**.

| Elemento | Conexión |
|---|---|
| PC817 pin 1 A | GPIO6 mediante 220 Ω |
| PC817 pin 2 K | GND_ESP |
| PC817 pin 4 C | Gate IRLZ44N y pull-up 10 kΩ a +5 V motor |
| PC817 pin 3 E | GND_motor |
| IRLZ44N pin 1 G | Nodo de gate anterior |
| IRLZ44N pin 2 D, también aleta | Motor negativo y ánodo del flyback |
| IRLZ44N pin 3 S | GND_motor |
| Motor positivo | +5 V motor y cátodo (banda) del flyback |
| TCRT5000 LED A / K | A por 220 Ω a 3.3 V; K a GND_ESP |
| TCRT5000 transistor C | Nodo A: pull-up 10 kΩ a 3.3 V; GPIO4 y GPIO10 directos |
| TCRT5000 transistor E | GND_ESP |
| CF, **único condensador del filtro** | 10 nF entre C (nodo A) y E (GND_ESP) |
| GPIO5 OUTPUT | Marcador para CH2 del osciloscopio; no conectar al sensor |

El condensador dibujado entre nodo A y GND_ESP **es el mismo** que se coloca entre
C y E, no un segundo condensador en paralelo. GPIO4 usa `INPUT`, sin pull-up
interno adicional. C1/C2 de P4 ya no se utilizan: no conectar sus señales de 24 V
a estos GPIO. No aplicar 5 V a GPIO4 ni GPIO10. El motor mantiene su fuente de 5 V
separada y su diodo flyback, dimensionado para su corriente.

IRLZ44N: frente marcado hacia el observador, patas hacia abajo, G-D-S de izquierda
a derecha. PC817: identificar la marca de pin 1. TCRT5000: usar la vista superior
Vishay para A/K/C/E; no deducirla de la longitud de las patas. La práctica emplea
sensor discreto, no módulo con comparador/salida DO.

El pull-up de gate de C3 **puede activar el motor durante reset, carga o pérdida de
alimentación del ESP32**. `setup()` solicita GPIO6 HIGH como primera acción y
PWM 0 %, pero no garantiza apagado por hardware durante reset. Conectar la fuente
del motor al final y quitarla antes de reiniciar, cargar o recablear. El USB puede
reiniciar la placa al abrir el monitor según configuración.

## Qué hace el RC

Más reflexión IR → más corriente óptica → colector bajo sobre blanco. Negro:
colector alto. Por ello oscuro→blanco produce el **FALLING** que se cuenta. El
disco tiene cuatro blancos separados por cuatro negros, ocho sectores iguales,
sobre el eje de salida del reductor: **4 PPR** contando solo este flanco.

Con 10 kΩ y 10 nF, la subida tiene constante aproximada **100 µs** y el RC lineal
equivalente tiene frecuencia de corte ideal **1.59 kHz**. La caída depende de
corriente óptica, saturación y geometría. El RC puede atenuar perturbaciones
breves, pero **no garantiza eliminar ruido de PWM a 1 kHz ni evita por sí solo
recruces del umbral digital**. No se añade histéresis externa. Una señal lenta o
ruidosa puede provocar varias interrupciones: hay que medirla y documentarla.

La tabla DC del ESP32-S3 a 3.3 V/25 °C especifica `VIL_max=0.25×VDD=0.825 V`
y `VIH_min=0.75×VDD=2.475 V`. Verificar colector por debajo del límite LOW sobre
blanco y por encima del límite HIGH sobre negro, con margen. Son límites para
reconocer niveles lógicos; **no son dos umbrales Schmitt exactos ni prometen
histéresis**. La zona intermedia no define un nivel lógico garantizado.

A 600 RPM y 4 PPR son 40 pulsos/s y unos 12.5 ms por sector de igual tamaño. Es
un ejemplo de escala respecto al RC, no un ensayo físico ni una capacidad máxima
garantizada del sistema.

## Programa y comandos

Seleccionar **ESP32S3 Dev Module**, Arduino-ESP32 **3.x**. Abrir el `.ino` con
`tacometro.h` en la misma carpeta, cargar sin fuente de motor y abrir monitor a
**115200 bps**, terminación Nueva línea. No requiere bibliotecas adicionales.

| Comando, en minúsculas | Efecto |
|---|---|
| `ayuda` | Sintaxis |
| `vel 0` … `vel 100` | PWM solicitado, cancela rampa |
| `stop` | PWM solicitado 0 %, cancela rampa; esperar que termine la inercia |
| `estado` | PWM, PPR, mV, IRQ y contadores de la captura |
| `leer` | Últimos mV de diagnóstico, IRQ y contadores |
| `stream on` / `stream off` | Activar/desactivar CSV de RPM aceptadas |
| `ppr 1` … `ppr 100` | Pulsos/vuelta; reinicia ventana atómicamente |
| `filtro 0` … `filtro 100000` | Tiempo muerto en µs, inicial 500; 0 desactiva |
| `rampa` | 0→100→0 %, pasos de 10 % cada 3000 ms, cancelable con `stop`/`vel` |
| `umbral ...` | ERROR: no existe ajuste de umbral en esta arquitectura |

`filtro` solo se acepta con PWM solicitado 0 % y rampa inactiva. El firmware no
comprueba reposo mecánico: enviar `stop` y **esperar que el motor se detenga** antes
de cambiarlo. El cambio actualiza filtro, contador de ventana e instante inicial
bajo la misma mux; conserva totales y la referencia del último aceptado. No
reinicia la bandera de primer pulso, para no permitir que el cambio eluda el
tiempo muerto. No crea flancos ni altera físicamente la señal.

Rampa nominal de unos 63 s, incluidos tres segundos en cero inicial y final.
Parser estricto: rechaza signos, decimales, valores fuera de rango y argumentos
extra. Búfer de 95 caracteres útiles: descarta completa una línea excesiva o no
ASCII; no ejecuta líneas sin LF. No usa `readStringUntil()`, `delay()` ni esperas
del puerto. Parámetros volátiles: vuelven al valor inicial al reiniciar.

GPIO10 es **ADC1_CH9**, 12 bits, `ADC_11db`, `analogReadMilliVolts()`. Su rango útil
aproximado alcanza 3100 mV; el nodo puede llegar a casi 3.3 V sobre negro y recortar
la lectura ADC. No exceder 3.3 V eléctricos. La última muestra puede tener unos
100 ms de antigüedad o más si el bucle se demora: no es osciloscopio ni mide cada
pulso. Nivel GPIO y mV de `leer` no son lecturas simultáneas de un flanco.

## ISR, contadores y marcador para osciloscopio

`attachInterrupt(digitalPinToInterrupt(4), contarPulso, FALLING)` registra la ISR
`ARDUINO_ISR_ATTR`. Registrar no suma pulsos. Cada interrupción atendida incrementa
`crudos_total`. Se acepta si es la primera o si ha transcurrido al menos `filtro_us`
desde el anterior aceptado; con filtro 0 se aceptan todas las ISR atendidas. Un
rechazo no desplaza la referencia temporal. `pulsos_total` contiene los aceptados;
`rechazados_total = crudos_total - pulsos_total`, con resta modular de 32 bits.
Los contadores describen IRQ atendidas: no detectan flancos perdidos antes de
ejecutar la ISR por límites del hardware o del sistema.

Ejemplo, filtro 500: flancos a 0, 100, 600, 900 y 1200 µs → acepta 0, 600 y 1200.
Es **tiempo muerto**, no validación del ancho de pulso: el primer pico espurio
puede pasar, y otro separado más de 500 µs también. Su límite aritmético es 2000
aceptados/s, o 30000 RPM a 4 PPR; **no es capacidad garantizada** de sensor,
montaje, procesador ni filtro. Con filtro 0 se desactiva esta protección para
comparar el fenómeno observado, sin garantizar capturar toda perturbación.

GPIO5 inicia LOW antes de conectar la ISR y **conmuta cada vez que se acepta**.
Cada cambio (subida o bajada) equivale a un aceptado; un rechazo no cambia GPIO5.
La frecuencia de su onda cuadrada es la mitad de la frecuencia de pulsos:
no calcular RPM contando solo RISING del marcador sin ese factor. Un pin alto
no significa que el sensor esté sobre blanco; es una marca de paridad del conteo.

ISR: timestamp, cuentas, referencia y escritura de marcador; sin Serial, ADC,
flotantes ni asignación dinámica. El core instalado 3.3.12 declara `__digitalWrite`
con `ARDUINO_ISR_ATTR`; GPIO5 se configura OUTPUT antes de `attachInterrupt`.
La escritura y el acceso de la API añaden trabajo a la ISR; esto no convierte
el montaje en un sistema con latencia máxima garantizada.

Variables compartidas `volatile` protegidas por el mismo `portMUX_TYPE`: ISR usa
`portENTER_CRITICAL_ISR`; tarea, `portENTER_CRITICAL`. Volatile por sí solo no basta.
`tomarVentana()` captura tiempo/cuentas y reinicia el contador e instante de ventana
en una sola sección crítica. `leerConteo()` captura los contadores consistentemente.
Cambiar PWM/PPR reinicia solo la ventana; los totales y filtro conservan continuidad.
Cálculos y mensajes ocurren después de liberar la mux. TX usa cola no bloqueante:
si se llena, omite mensajes completos y aumenta `tx_omitidos` en `estado`.

## Evidencia obligatoria: comparación 2 × 2 con osciloscopio

**CH1 al nodo A/GPIO4; CH2 a GPIO5; ambas referencias a GND_ESP.** No conectar una
tierra del mismo osciloscopio al lado motor: uniría las tierras aisladas. Usar
sondas ×10 y conexiones de tierra cortas cuando estén disponibles. Observar la
forma de onda y los cambios de marca en una captura completa y una ampliación de
transiciones. Registrar escalas, tiempo, PWM, PPR, alimentación y condición óptica.

Comparar las cuatro condiciones, conservando giro comparable con la misma
consigna y condiciones mecánicas. No suponer que igual PWM garantiza iguales RPM.

| Condición | CF entre C y E | Comando | Evidencia a guardar |
|---|---|---|---|
| 1 | Ausente | `filtro 0` | CH1/CH2, duración y diferencias de los tres totales |
| 2 | Ausente | `filtro 500` | Igual registro |
| 3 | 10 nF, único | `filtro 0` | Igual registro |
| 4 | 10 nF, único | `filtro 500` | Igual registro |

Para cada cambio: `stop`, esperar reposo y quitar fuente de motor antes de tocar
CF. Reponer/retirar **el mismo** condensador; no añadir otro en paralelo. Cambiar
filtro en reposo. Reconectar fuente al final, restaurar consigna y esperar régimen
antes de capturar. No quitar flyback, puentear tierras ni inducir fallas para
producir ruido. Si no aparecen eventos espurios o rechazados, **documentar cero**:
la evidencia no exige que cada condición tenga ruido.

Anotar `estado` antes/después del intervalo y calcular diferencias modulares de
`crudos_total`, `pulsos_total` y `rechazados_total`; verificar
`Δcrudos = Δaceptados + Δrechazados`. Usar la diferencia de `tiempo_ms` de esos
estados para la duración real: se captura con los totales bajo la misma mux.
No usar la diferencia de `dt_ms` parcial, pues la ventana se reinicia cada segundo.
Comparar con CH1/CH2: el filtro no cambia
la forma de onda analógica de CH1; CF sí puede modificarla. No contar las cuentas
del intervalo de arranque, cambio de CF o configuración como datos en régimen.

Validación inicial sin fuente de motor: situar el disco, anotar `pulsos_total`, dar
una vuelta manual lenta y comprobar diferencia 4. Ajustar separación alrededor
de 2.5 mm inicialmente y comprobar contraste IR real. La inspección visible del
disco no garantiza contraste infrarrojo. Corregir conexiones/montaje antes de
motorizar. Conectar fuente motor al final tras `stop`.

## RPM, CSV y reutilización de C3

Ventana de al menos 1000 ms, duración real mediante resta modular:

`RPM = 60000 × pulsos_aceptados / (ppr × dt_ms)`

20 aceptados/4 PPR/1000 ms → 300 RPM; con los mismos pulsos en 1250 ms → 240 RPM.
Resolución a 4 PPR/1 s: **15 RPM por pulso**, no exactitud garantizada. Dos decimales
no aumentan esa resolución. Cero aceptados puede indicar giro lento, mal contraste
o desconexión y no demuestra por sí solo reposo. Un canal no mide dirección.

Encabezado CSV estable, siete campos:

```csv
tiempo_ms,pwm_pct,mV,pulsos,ppr,dt_ms,rpm
```

`tiempo_ms`: instante de captura; `pwm_pct`: consigna, no porcentaje de velocidad;
`mV`: última muestra lenta, no promedio ni cada flanco; `pulsos`: **aceptados de
esa ventana**; `ppr`, `dt_ms`: valores usados para la cuenta; `rpm`: resultado.
Los totales de crudos/aceptados/rechazados aparecen fuera del CSV en `estado`/`leer`.
Excluir mensajes que empiezan con `#` al importar. `stream on` repite encabezado
y no reinicia ventana; su primera fila puede incluir adquisición previa. El sello
corresponde a la captura aunque TX entregue la fila posteriormente.

Motor: PWM 1 kHz, 10 bits, `dutyPin=1023-floor(pwm_pct×1023/100)` por la inversión
PC817/MOSFET. PWM 0 % → GPIO6 HIGH → PC817 conduce → gate baja → motor apagado;
100 % invierte esa cadena. Verificar tiempos reales de PC817/gate. Fallos
`ledcAttach`/`ledcWrite` inhiben consignas y piden HIGH como mejor esfuerzo,
sin garantizar apagado seguro por hardware. P4 original no se modificó.

## Verificación reproducible y límites

Desde esta carpeta:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32s3 practica5_tacometro_optico
python tests/test_host.py
pdflatex -interaction=nonstopmode -halt-on-error diagrama-bloques-v3.tex
pdflatex -interaction=nonstopmode -halt-on-error diagrama-conexiones-v3.tex
```

Arduino CLI 1.5.1 / Arduino-ESP32 3.3.12, FQBN `esp32:esp32:esp32s3`: compilación
correcta, **328193 bytes de programa y 25376 bytes globales**. Se comprueba
compilación real y pruebas host del sketch: IRQ FALLING sin pulso inicial inventado,
crudos/aceptados/rechazados, filtro 0..100000 y bloqueos en marcha/rampa, marcador
GPIO5, conservación de referencia, tiempo real y desbordamientos, parser/RX,
rampa y fallo PWM. Prueba concurrente de 20000 ISR simuladas frente a capturas y
resets: conservación del conteo y una conmutación de GPIO5 por aceptado.

**No se han realizado ensayos físicos ni las cuatro capturas de osciloscopio.**
Son evidencia que deberán obtener en el laboratorio; no se incluyen datos
inventados. Faltan comprobar ruido, formas de onda, contraste, corriente de
arranque, estabilidad mecánica y RPM contra referencia. Los hilos host no simulan
latencias reales de la tarjeta ni certifican capacidad de interrupciones.

## Referencias primarias

- [Espressif GPIO y attachInterrupt](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/gpio.html).
- [Espressif ESP32-S3: tabla DC de niveles digitales](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf).
- [Espressif implementación GPIO Arduino-ESP32 3.3.12](https://github.com/espressif/arduino-esp32/blob/3.3.12/cores/esp32/esp32-hal-gpio.c).
- [Espressif LEDC](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ledc.html).
- [Espressif ADC](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/adc.html).
- [Vishay TCRT5000](https://www.vishay.com/docs/83760/tcrt5000.pdf).
- [Infineon IRLZ44N](https://www.infineon.com/assets/row/public/documents/24/49/infineon-irlz44n-datasheet-en.pdf).
- PC817: cableado conservado de C3/P4; verificar hoja de la marca instalada.

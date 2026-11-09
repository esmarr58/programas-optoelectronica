// =====================================================================
//  Optoelectronica V3736 · Practica 6 · Amplificador de transimpedancia
//  BPW34 + TL072 (+/-12 V) con LED rojo, verde y azul · ESP32-S3
//  Dr. Ruben Estrada Marmolejo · CUCEI-UDG · 2026B
//
//  Un paso del espectrometro: enciende un color a la vez, espera a que la
//  senal se estabilice, promedia el ADC, resta la lectura en oscuridad y
//  convierte el voltaje del TIA en fotocorriente:  I_ph = V_neto / R_f.
//
//  Conexiones (ver diagrama-conexiones.pdf):
//    GPIO4  <- 10 kohm <- salida TL072 (pin 1). Nodo ADC con Schottky a 3V3
//              y a GND, y 100 nF a GND. NUNCA la salida directa al GPIO.
//    GPIO5  -> 4.7 kohm -> base NPN del LED rojo
//    GPIO6  -> 4.7 kohm -> base NPN del LED verde
//    GPIO7  -> 4.7 kohm -> base NPN del LED azul
//    Cada LED: +12 V -> 1 kohm -> LED -> colector NPN; emisor a GND.
//    GND de la ESP32 = punto medio (0 V) de la fuente dual.
//
//  El programa NO cambia el modo ni R_f: eso lo hace el puente J1
//  (FV: anodo a GND; FC: anodo a -12 V por 10 kohm) y la R_f que
//  coloques. Los comandos "modo" y "rf" solo informan al programa.
//
//  Monitor Serial: 115200 baudios, "Nueva linea". Escribe: ayuda
//  Placa: ESP32S3 Dev Module · core Arduino-ESP32 3.x
// =====================================================================
#include <Arduino.h>

// ---------- Pines ----------
constexpr int PIN_ADC = 4;                      // ADC1_CH3
constexpr int NUM_LEDS = 3;
constexpr int PIN_LED[NUM_LEDS] = {5, 6, 7};    // rojo, verde, azul
const char *NOMBRE_LED[NUM_LEDS] = {"rojo", "verde", "azul"};

// ---------- Limites del ADC (ESP32-S3, atenuacion 11/12 dB) ----------
constexpr int MV_SATURA = 2900;   // arriba de esto el ADC ya no es confiable
constexpr int MV_BAJO = 50;       // abajo de esto el ADC resuelve mal: usa multimetro

// ---------- Parametros que el alumno puede cambiar ----------
uint32_t rfOhm = 100000;          // R_f colocada (solo para calcular I_ph)
uint16_t nProm = 64;              // muestras por lectura
uint16_t esperaMs = 300;          // espera tras cambiar el LED
char modo[3] = "fv";              // fv = fotovoltaico, fc = fotoconductivo

// ---------- Estado ----------
int ledActivo = -1;               // -1 = todos apagados
int ledPulso = -1;                // LED en modo pulso (-1 = ninguno)
int32_t mvOscuro = 0;
bool hayOscuro = false;
bool stream = false;
uint32_t ultimoStream = 0;
char linea[48];
uint8_t largo = 0;

// ---------------------------------------------------------------------
void apagarLeds() {
  for (int i = 0; i < NUM_LEDS; ++i) digitalWrite(PIN_LED[i], LOW);
  ledActivo = -1;
}

void encenderLed(int i) {
  apagarLeds();
  if (i >= 0 && i < NUM_LEDS) {
    digitalWrite(PIN_LED[i], HIGH);
    ledActivo = i;
  }
}

// Promedio de nProm lecturas calibradas (mV en el pin, no en la salida del TL072)
int32_t leerMv() {
  int64_t suma = 0;
  for (uint16_t k = 0; k < nProm; ++k) {
    suma += analogReadMilliVolts(PIN_ADC);
    delayMicroseconds(200);
  }
  return (int32_t)(suma / nProm);
}

const char *nombreActivo() { return ledActivo < 0 ? "oscuro" : NOMBRE_LED[ledActivo]; }

const char *diagnostico(int32_t mv) {
  if (mv >= MV_SATURA) return "SATURA";
  if (mv <= MV_BAJO) return "BAJO";
  return "ok";
}

// tiempo_ms,modo,rf_ohm,led,mV,mV_osc,mV_neto,I_uA,estado
void imprimirFila(const char *led, int32_t mv) {
  const int32_t osc = hayOscuro ? mvOscuro : 0;
  const int32_t neto = mv - osc;
  const float iuA = (float)neto * 1000.0f / (float)rfOhm;   // mV/ohm = mA -> x1000 = uA
  Serial.printf("%lu,%s,%lu,%s,%ld,%ld,%ld,%.4f,%s\n", (unsigned long)millis(), modo,
                (unsigned long)rfOhm, led, (long)mv, (long)osc, (long)neto, iuA, diagnostico(mv));
}

void encabezado() { Serial.println(F("tiempo_ms,modo,rf_ohm,led,mV,mV_osc,mV_neto,I_uA,estado")); }

void medirOscuro() {
  apagarLeds();
  delay(esperaMs);
  mvOscuro = leerMv();
  hayOscuro = true;
  imprimirFila("oscuro", mvOscuro);
}

void secuencia() {
  encabezado();
  medirOscuro();
  for (int i = 0; i < NUM_LEDS; ++i) {
    encenderLed(i);
    delay(esperaMs);
    imprimirFila(NOMBRE_LED[i], leerMv());
  }
  apagarLeds();
}

void detenerPulso() {
  if (ledPulso >= 0) {
    ledcDetach(PIN_LED[ledPulso]);
    pinMode(PIN_LED[ledPulso], OUTPUT);
    digitalWrite(PIN_LED[ledPulso], LOW);
    ledPulso = -1;
  }
}

int indiceLed(char c) {
  if (c == 'r') return 0;
  if (c == 'g' || c == 'v') return 1;
  if (c == 'b' || c == 'a') return 2;
  return -1;
}

void ayuda() {
  Serial.println(F("\n=== P6 · TIA con BPW34 · LED rojo, verde y azul ==="));
  Serial.println(F("ayuda            esta lista"));
  Serial.println(F("estado           parametros y oscuro guardado"));
  Serial.println(F("led r|g|b|off    enciende un color (g = verde) o apaga"));
  Serial.println(F("leer             una lectura promediada con el LED actual"));
  Serial.println(F("oscuro           apaga LEDs, mide y guarda la referencia"));
  Serial.println(F("secuencia        oscuro, rojo, verde, azul (CSV)"));
  Serial.println(F("stream on|off    lectura cada 0.5 s (para alinear)"));
  Serial.println(F("modo fv|fc       etiqueta del modo que armaste con J1"));
  Serial.println(F("rf N             R_f colocada en ohm (1000..100000000)"));
  Serial.println(F("prom N           muestras por lectura (1..1024)"));
  Serial.println(F("espera N         ms tras cambiar LED (50..5000)"));
  Serial.println(F("csv              imprime el encabezado del CSV"));
  Serial.println(F("pulso r|g|b F    onda cuadrada de F Hz (5..20000) en un LED"));
  Serial.println(F("pulso off        detiene la onda cuadrada"));
  Serial.println(F("El ADC lee el nodo protegido (0..2.9 V). Compara con el multimetro en la salida del TL072."));
}

void estado() {
  Serial.printf("modo=%s rf_ohm=%lu prom=%u espera_ms=%u led=%s pulso=%s oscuro=",
                modo, (unsigned long)rfOhm, nProm, esperaMs, nombreActivo(),
                ledPulso < 0 ? "off" : NOMBRE_LED[ledPulso]);
  if (hayOscuro) Serial.printf("%ld mV\n", (long)mvOscuro);
  else Serial.println(F("sin medir (usa: oscuro)"));
}

// ---------------------------------------------------------------------
void procesar(char *cmd) {
  for (char *p = cmd; *p; ++p) *p = (char)tolower(*p);
  char *arg = strchr(cmd, ' ');
  if (arg) { *arg++ = '\0'; while (*arg == ' ') ++arg; }

  const bool hayPulso = ledPulso >= 0;
  if (!strcmp(cmd, "ayuda")) { ayuda(); return; }
  if (!strcmp(cmd, "estado")) { estado(); return; }

  if (!strcmp(cmd, "pulso")) {
    if (arg && !strcmp(arg, "off")) { detenerPulso(); Serial.println(F("pulso detenido")); return; }
    int i = arg ? indiceLed(arg[0]) : -1;
    long f = (arg && strlen(arg) > 1) ? atol(arg + 1) : 0;
    if (i < 0 || f < 5 || f > 20000) { Serial.println(F("uso: pulso r|g|b F  (F de 5 a 20000 Hz)")); return; }
    detenerPulso(); apagarLeds(); stream = false;
    // Abajo de ~300 Hz el divisor del LEDC no alcanza con 8 bits: se usa mas resolucion.
    const uint8_t res = (f < 1000) ? 14 : 8;
    if (!ledcAttach(PIN_LED[i], (uint32_t)f, res)) {
      pinMode(PIN_LED[i], OUTPUT); digitalWrite(PIN_LED[i], LOW);
      Serial.println(F("error: LEDC no acepto esa frecuencia"));
      return;
    }
    ledcWrite(PIN_LED[i], 1u << (res - 1));       // 50 % de ciclo de trabajo
    ledPulso = i;
    Serial.printf("pulso %s a %ld Hz (50 %%). Mide la salida del TL072 con el osciloscopio.\n", NOMBRE_LED[i], f);
    return;
  }
  if (hayPulso) { Serial.println(F("primero: pulso off")); return; }

  if (!strcmp(cmd, "led")) {
    if (arg && !strcmp(arg, "off")) { apagarLeds(); Serial.println(F("LEDs apagados")); return; }
    int i = arg ? indiceLed(arg[0]) : -1;
    if (i < 0) { Serial.println(F("uso: led r|g|b|off")); return; }
    encenderLed(i);
    Serial.printf("LED %s encendido\n", NOMBRE_LED[i]);
    return;
  }
  // leer y oscuro no repiten el encabezado: el CSV pegado queda limpio (usa: csv)
  if (!strcmp(cmd, "leer")) { imprimirFila(nombreActivo(), leerMv()); return; }
  if (!strcmp(cmd, "oscuro")) { medirOscuro(); return; }
  if (!strcmp(cmd, "csv")) { encabezado(); return; }
  if (!strcmp(cmd, "secuencia")) { stream = false; secuencia(); return; }
  if (!strcmp(cmd, "stream")) {
    stream = arg && !strcmp(arg, "on");
    if (stream) encabezado();
    Serial.println(stream ? F("stream on") : F("stream off"));
    return;
  }
  if (!strcmp(cmd, "modo")) {
    if (arg && (!strcmp(arg, "fv") || !strcmp(arg, "fc"))) {
      strncpy(modo, arg, 2); modo[2] = '\0';
      hayOscuro = false;                          // el oscuro cambia con el modo
      Serial.printf("modo=%s (recuerda mover J1). Vuelve a medir: oscuro\n", modo);
    } else Serial.println(F("uso: modo fv|fc"));
    return;
  }
  if (!strcmp(cmd, "rf")) {
    long v = arg ? atol(arg) : 0;
    if (v < 1000 || v > 100000000L) { Serial.println(F("uso: rf N  (1000..100000000 ohm)")); return; }
    rfOhm = (uint32_t)v; hayOscuro = false;
    Serial.printf("rf_ohm=%lu. Vuelve a medir: oscuro\n", (unsigned long)rfOhm);
    return;
  }
  if (!strcmp(cmd, "prom")) {
    long v = arg ? atol(arg) : 0;
    if (v < 1 || v > 1024) { Serial.println(F("uso: prom N  (1..1024)")); return; }
    nProm = (uint16_t)v; Serial.printf("prom=%u\n", nProm); return;
  }
  if (!strcmp(cmd, "espera")) {
    long v = arg ? atol(arg) : 0;
    if (v < 50 || v > 5000) { Serial.println(F("uso: espera N  (50..5000 ms)")); return; }
    esperaMs = (uint16_t)v; Serial.printf("espera_ms=%u\n", esperaMs); return;
  }
  if (*cmd) Serial.println(F("comando desconocido; escribe: ayuda"));
}

void setup() {
  for (int i = 0; i < NUM_LEDS; ++i) { pinMode(PIN_LED[i], OUTPUT); digitalWrite(PIN_LED[i], LOW); }
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_ADC, ADC_11db);     // rango aprox. 0..3.1 V
  delay(500);
  ayuda();
}

void loop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') { linea[largo] = '\0'; procesar(linea); largo = 0; }
    else if (largo < sizeof(linea) - 1) linea[largo++] = c;
  }
  if (stream && ledPulso < 0 && millis() - ultimoStream >= 500) {
    ultimoStream = millis();
    imprimirFila(nombreActivo(), leerMv());
  }
}

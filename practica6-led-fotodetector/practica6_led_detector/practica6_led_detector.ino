// =====================================================================
//  Optoelectronica V3736 · Practica 6 · El LED como fotodetector
//  TIA con LM358 a una sola fuente de 5 V · ESP32-S3
//  Dr. Ruben Estrada Marmolejo · CUCEI-UDG · 2026B
//
//  Antesala del fotodiodo: un LED rojo, verde o azul hace de DETECTOR y otro
//  LED (rojo, verde, azul o blanco) lo ilumina. Matriz de 3 x 4 combinaciones,
//  en modo fotovoltaico (FV) y fotoconductivo (FC).
//
//  El TIA trabaja sobre un pedestal V_ref ~ 1.0 V (mitad B del LM358 como
//  seguidor), asi la salida nunca choca con 0 V:
//      V_o = V_ref + (I_ph - I_B) * R_f
//  El comando "medir" apaga el emisor (oscuro), lo enciende (luz) y resta:
//      I_ph = (V_o,luz - V_o,osc) / R_f        (V_ref e I_B se cancelan)
//
//  Conexiones (ver diagrama-conexiones.pdf):
//    GPIO4 <- divisor 10 k / 22 k <- salida LM358 pin 1 (V_o max ~3.7 V -> 2.5 V)
//    GPIO6 <- 10 k <- V_ref (pin 7, salida del seguidor) (~1.0 V)
//    GPIO5 -> 4.7 k -> base del NPN del LED EMISOR (+5 V -> 220 ohm -> LED -> NPN)
//    LM358 pin 8 = 5V de la placa, pin 4 = GND.
//  El programa NO cambia el modo, R_f ni los LED: eso lo haces tu en la
//  protoboard. Los comandos det, emi, modo y rf solo etiquetan y calculan.
//
//  Monitor Serial: 115200 baudios, "Nueva linea". Escribe: ayuda
//  Placa: ESP32S3 Dev Module · core Arduino-ESP32 3.x
// =====================================================================
#include <Arduino.h>

// ---------- Pines ----------
constexpr int PIN_VO = 4;      // ADC1_CH3, salida del TIA por el divisor
constexpr int PIN_VREF = 6;    // ADC1_CH5, pedestal V_ref
constexpr int PIN_EMI = 5;     // base del NPN del emisor

// ---------- Divisor de la salida ----------
constexpr float K_DIV = (10000.0f + 22000.0f) / 22000.0f;   // 1.4545

// ---------- Limites ----------
constexpr float MV_SATURA = 3100.0f;   // con USB de 4.75-5 V el LM358 satura entre ~3.25 y 3.5 V
constexpr float MV_PISO = 150.0f;      // V_o cerca de 0 V: I_B*R_f se comio el pedestal
constexpr float MV_INVERTIDO = -20.0f; // luz que BAJA la salida: detector al reves

// ---------- Etiquetas ----------
const char *DET[3] = {"rojo", "verde", "azul"};
const char *EMI[4] = {"rojo", "verde", "azul", "blanco"};
const char *MODO[2] = {"fv", "fc"};

// ---------- Parametros ----------
uint32_t rfOhm = 10000000;   // R_f colocada (10 Mohm al iniciar)
uint16_t nProm = 64;         // muestras por lectura
uint16_t esperaMs = 500;     // espera tras encender/apagar el emisor (R_f*C_f = 10 ms)
uint8_t nRep = 3;            // repeticiones por "medir"
int det = 0, emi = 0, modo = 0;

// ---------- Matriz de resultados (promedio en nA) ----------
float tablaI[2][3][4];
float tablaDisp[2][3][4];
bool tablaOk[2][3][4];
bool tablaDudosa[2][3][4];   // alguna repeticion no salio "ok"

bool stream = false;
uint32_t ultimoStream = 0;
char linea[48];
uint8_t largo = 0;

// ---------------------------------------------------------------------
float leerMv(int pin) {
  int64_t suma = 0;
  for (uint16_t k = 0; k < nProm; ++k) {
    suma += analogReadMilliVolts(pin);
    delayMicroseconds(200);
  }
  return (float)suma / nProm;
}
float leerVo() { return leerMv(PIN_VO) * K_DIV; }   // mV en la salida del LM358
float leerVref() { return leerMv(PIN_VREF); }

void emisor(bool on) { digitalWrite(PIN_EMI, on ? HIGH : LOW); }

const char *diagnostico(float vOsc, float vLuz, float neto) {
  if (vLuz >= MV_SATURA) return "SATURA";
  if (vOsc <= MV_PISO) return "PISO";
  if (neto <= MV_INVERTIDO) return "INVERTIDO?";
  return "ok";
}

void encabezado() {
  Serial.println(F("tiempo_ms,modo,rf_ohm,detector,emisor,rep,Vref_mV,Vo_osc_mV,Vo_luz_mV,Vo_neto_mV,I_nA,estado"));
}

// Mide nRep veces: oscuro -> luz -> resta. Guarda el promedio en la matriz.
// Resta tambien el cambio de V_ref: al encender el emisor cae un poco el riel de 5 V
// y V_ref se mueve junto con la luz; ese corrimiento no es fotocorriente.
void medir() {
  float suma = 0, mn = 1e30f, mx = -1e30f;
  bool todoOk = true;
  for (uint8_t r = 1; r <= nRep; ++r) {
    emisor(false); delay(esperaMs);
    const float vOsc = leerVo();
    const float vRefOsc = leerVref();
    emisor(true); delay(esperaMs);
    const float vLuz = leerVo();
    const float vRef = leerVref();
    emisor(false);
    const float neto = (vLuz - vRef) - (vOsc - vRefOsc);
    const float iNa = neto * 1.0e6f / (float)rfOhm;      // mV/ohm = mA -> x1e6 = nA
    const char *est = diagnostico(vOsc, vLuz, neto);
    if (strcmp(est, "ok")) todoOk = false;
    Serial.printf("%lu,%s,%lu,%s,%s,%u,%.0f,%.1f,%.1f,%.1f,%.3f,%s\n", (unsigned long)millis(), MODO[modo],
                  (unsigned long)rfOhm, DET[det], EMI[emi], r, vRef, vOsc, vLuz, neto, iNa, est);
    suma += iNa; if (iNa < mn) mn = iNa; if (iNa > mx) mx = iNa;
  }
  const float prom = suma / nRep;
  tablaI[modo][det][emi] = prom;
  tablaDisp[modo][det][emi] = mx - mn;
  tablaOk[modo][det][emi] = true;
  tablaDudosa[modo][det][emi] = !todoOk;
  Serial.printf("# promedio %s: detector %s, emisor %s = %.3f nA (max-min %.3f nA)%s\n", MODO[modo], DET[det],
                EMI[emi], prom, mx - mn, todoOk ? "" : "  <- revisa el estado");
}

void imprimirTabla() {
  for (int m = 0; m < 2; ++m) {
    Serial.printf("\n# Matriz I_ph (nA), modo %s, filas = detector, columnas = emisor\n", MODO[m]);
    Serial.println(F("detector,rojo,verde,azul,blanco"));
    for (int d = 0; d < 3; ++d) {
      Serial.print(DET[d]);
      for (int e = 0; e < 4; ++e) {
        if (tablaOk[m][d][e]) Serial.printf(",%.3f%s", tablaI[m][d][e], tablaDudosa[m][d][e] ? "*" : "");
        else Serial.print(",--");
      }
      Serial.println();
    }
  }
  Serial.println(F("# * = alguna repeticion no salio \"ok\" (SATURA, PISO o INVERTIDO?): repitela"));
}

void ayuda() {
  Serial.println(F("\n=== P6 · El LED como fotodetector · TIA con LM358 (0-5 V) ==="));
  Serial.println(F("ayuda / estado      esta lista / parametros actuales"));
  Serial.println(F("det rojo|verde|azul           LED que pusiste como DETECTOR"));
  Serial.println(F("emi rojo|verde|azul|blanco    LED que pusiste como EMISOR"));
  Serial.println(F("   (tambien: ro, ve, az, bl)"));
  Serial.println(F("modo fv|fc          posicion de J1: fv = anodo a V_ref, fc = anodo a GND"));
  Serial.println(F("rf N                R_f colocada en ohm (100000..100000000)"));
  Serial.println(F("medir               oscuro -> luz -> resta, rep veces; guarda en la matriz"));
  Serial.println(F("tabla               imprime la matriz 3 x 4 en fv y en fc"));
  Serial.println(F("borrar              borra la matriz"));
  Serial.println(F("leer                V_o y V_ref ahora (sin cambiar el emisor)"));
  Serial.println(F("led on|off          enciende o apaga el emisor a mano"));
  Serial.println(F("stream on|off       V_o cada 0.5 s (para alinear)"));
  Serial.println(F("rep N / prom N      repeticiones (1..10) / muestras por lectura (1..1024)"));
  Serial.println(F("espera N            ms tras encender o apagar el emisor (50..5000)"));
  Serial.println(F("csv                 imprime el encabezado del CSV (una vez, al empezar)"));
}

void estado() {
  Serial.printf("detector=%s emisor=%s modo=%s rf_ohm=%lu rep=%u prom=%u espera_ms=%u\n", DET[det], EMI[emi],
                MODO[modo], (unsigned long)rfOhm, nRep, nProm, esperaMs);
}

// Acepta la palabra completa o sus dos primeras letras: rojo/ro, verde/ve, azul/az, blanco/bl.
int indiceColor(const char *arg, int n) {
  static const char *pref[4] = {"ro", "ve", "az", "bl"};
  if (!arg || strlen(arg) < 2) return -1;
  for (int i = 0; i < n; ++i)
    if (!strncmp(arg, pref[i], 2)) return i;
  return -1;
}

void procesar(char *cmd) {
  for (char *p = cmd; *p; ++p) *p = (char)tolower(*p);
  char *arg = strchr(cmd, ' ');
  if (arg) { *arg++ = '\0'; while (*arg == ' ') ++arg; }
  const long v = arg ? atol(arg) : 0;

  if (!strcmp(cmd, "ayuda")) { ayuda(); return; }
  if (!strcmp(cmd, "estado")) { estado(); return; }
  if (!strcmp(cmd, "csv")) { encabezado(); return; }
  if (!strcmp(cmd, "tabla")) { imprimirTabla(); return; }
  if (!strcmp(cmd, "borrar")) {
    memset(tablaOk, 0, sizeof(tablaOk)); memset(tablaDudosa, 0, sizeof(tablaDudosa));
    Serial.println(F("matriz borrada")); return;
  }
  if (!strcmp(cmd, "det")) {
    const int i = indiceColor(arg, 3);
    if (i < 0) { Serial.println(F("uso: det rojo|verde|azul")); return; }
    det = i; Serial.printf("detector=%s\n", DET[det]); return;
  }
  if (!strcmp(cmd, "emi")) {
    const int i = indiceColor(arg, 4);
    if (i < 0) { Serial.println(F("uso: emi rojo|verde|azul|blanco")); return; }
    emi = i; Serial.printf("emisor=%s\n", EMI[emi]); return;
  }
  if (!strcmp(cmd, "modo")) {
    if (arg && !strcmp(arg, "fv")) modo = 0;
    else if (arg && !strcmp(arg, "fc")) modo = 1;
    else { Serial.println(F("uso: modo fv|fc")); return; }
    Serial.printf("modo=%s (recuerda mover J1)\n", MODO[modo]); return;
  }
  if (!strcmp(cmd, "rf")) {
    if (v < 100000 || v > 100000000L) { Serial.println(F("uso: rf N  (100000..100000000 ohm)")); return; }
    rfOhm = (uint32_t)v; Serial.printf("rf_ohm=%lu\n", (unsigned long)rfOhm); return;
  }
  if (!strcmp(cmd, "rep")) {
    if (v < 1 || v > 10) { Serial.println(F("uso: rep N  (1..10)")); return; }
    nRep = (uint8_t)v; Serial.printf("rep=%u\n", nRep); return;
  }
  if (!strcmp(cmd, "prom")) {
    if (v < 1 || v > 1024) { Serial.println(F("uso: prom N  (1..1024)")); return; }
    nProm = (uint16_t)v; Serial.printf("prom=%u\n", nProm); return;
  }
  if (!strcmp(cmd, "espera")) {
    if (v < 50 || v > 5000) { Serial.println(F("uso: espera N  (50..5000 ms)")); return; }
    esperaMs = (uint16_t)v; Serial.printf("espera_ms=%u\n", esperaMs); return;
  }
  if (!strcmp(cmd, "medir")) { stream = false; medir(); return; }
  if (!strcmp(cmd, "leer")) {
    Serial.printf("Vo=%.1f mV  Vref=%.0f mV  emisor=%s\n", leerVo(), leerVref(),
                  digitalRead(PIN_EMI) ? "encendido" : "apagado");
    return;
  }
  if (!strcmp(cmd, "led")) {
    if (arg && !strcmp(arg, "on")) emisor(true);
    else if (arg && !strcmp(arg, "off")) emisor(false);
    else { Serial.println(F("uso: led on|off")); return; }
    Serial.println(digitalRead(PIN_EMI) ? F("emisor encendido") : F("emisor apagado")); return;
  }
  if (!strcmp(cmd, "stream")) {
    stream = arg && !strcmp(arg, "on");
    Serial.println(stream ? F("stream on") : F("stream off")); return;
  }
  if (*cmd) Serial.println(F("comando desconocido; escribe: ayuda"));
}

void setup() {
  pinMode(PIN_EMI, OUTPUT); emisor(false);
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_VO, ADC_11db);      // 0..3.1 V
  analogSetPinAttenuation(PIN_VREF, ADC_11db);
  delay(500);
  ayuda();
}

void loop() {
  while (Serial.available()) {
    const char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') { linea[largo] = '\0'; procesar(linea); largo = 0; }
    else if (largo < sizeof(linea) - 1) linea[largo++] = c;
  }
  if (stream && millis() - ultimoStream >= 500) {
    ultimoStream = millis();
    Serial.printf("Vo=%.1f mV  Vref=%.0f mV\n", leerVo(), leerVref());
  }
}

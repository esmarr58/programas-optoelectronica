/*
 * ============================================================================
 *  Contador óptico 0–9  ·  MODO DIGITAL (transistor en CORTE y SATURACIÓN)
 *  Práctica 2 · Optoelectrónica (V3736) · Dr. Rubén Estrada Marmolejo · CUCEI–UDG
 *  Placa: ESP32-S3  ·  Entorno: Arduino
 * ============================================================================
 *
 *  QUÉ HACE
 *  --------
 *  Cuenta 0..9 cada vez que un objeto CRUZA el haz del switch óptico (LED IR
 *  emisor  ->  fototransistor WP7113P3C) y muestra la cuenta en un display de
 *  7 segmentos de CÁTODO COMÚN. Al llegar a 9 vuelve a 0.
 *
 *  POR QUÉ "CORTE Y SATURACIÓN"
 *  ---------------------------
 *  El fototransistor se cablea en emisor común con R_L del colector a 3.3 V.
 *  La decisión 0/1 la hace el HARDWARE (ver Ejercicio 1 del deck de teoría):
 *    · Haz PRESENTE (llega luz)      -> el transistor SATURA -> V_CE ≈ 0.2–0.8 V
 *                                      -> el GPIO lee LOW (0 lógico).
 *    · Haz INTERRUMPIDO (objeto)     -> el transistor entra en CORTE -> V_CE ≈ 3.3 V
 *                                      -> el GPIO lee HIGH (1 lógico).
 *  Por eso aquí basta digitalRead(): la señal ya viene "cuadrada".
 *
 *  IMPORTANTE — VOLTAJE
 *  --------------------
 *  Alimenta el switch óptico a 3.3 V (NO 5 V): así el colector se conecta
 *  DIRECTO a un GPIO de la ESP32-S3 (máx 3.3 V) sin nivelador. R_L = 2.2 kΩ.
 *
 *  CABLEADO (resumen; ver README.md)
 *  ---------------------------------
 *    Fototransistor: colector -> R_L(2.2k) -> 3V3 ; colector -> GPIO8 ; emisor -> GND
 *    Display 7 seg (cátodo común): cada segmento a-g -> R(220–330Ω) -> su GPIO
 *                                  pin(es) común -> GND
 * ============================================================================
 */

// ------------------------- Pines -------------------------
// Segmentos del display en orden a, b, c, d, e, f, g
const uint8_t SEG[7] = {4, 5, 6, 7, 15, 16, 17};
const uint8_t PIN_SENSOR = 8;   // colector del fototransistor (entrada digital)

// ------------------ Patrones 0–9 -------------------------
// Cátodo común: segmento ENCENDIDO = HIGH.  Orden: a b c d e f g
const uint8_t patron[10][7] = {
  // a  b  c  d  e  f  g
  {1, 1, 1, 1, 1, 1, 0},  // 0
  {0, 1, 1, 0, 0, 0, 0},  // 1
  {1, 1, 0, 1, 1, 0, 1},  // 2
  {1, 1, 1, 1, 0, 0, 1},  // 3
  {0, 1, 1, 0, 0, 1, 1},  // 4
  {1, 0, 1, 1, 0, 1, 1},  // 5
  {1, 0, 1, 1, 1, 1, 1},  // 6
  {1, 1, 1, 0, 0, 0, 0},  // 7
  {1, 1, 1, 1, 1, 1, 1},  // 8
  {1, 1, 1, 1, 0, 1, 1},  // 9
};

// ------------------ Estado + antirrebote -----------------
int cuenta = 0;
const unsigned long ANTIRREBOTE_MS = 25;   // la señal debe mantenerse estable

void mostrar(int d) {
  for (int i = 0; i < 7; i++) digitalWrite(SEG[i], patron[d][i]);
}

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 7; i++) pinMode(SEG[i], OUTPUT);
  pinMode(PIN_SENSOR, INPUT);   // el nivel lo define R_L; no hace falta pull-up
  mostrar(cuenta);
  Serial.println("Contador optico - MODO DIGITAL (corte/saturacion). Cuenta = 0");
}

void loop() {
  // true  = haz interrumpido (transistor en corte, GPIO en HIGH)
  // false = haz presente     (transistor saturado, GPIO en LOW)
  bool lectura = (digitalRead(PIN_SENSOR) == HIGH);

  // --- Antirrebote por tiempo estable (patrón estándar) ---
  static bool estadoEstable = false;   // último estado ya confirmado
  static bool lecturaAnt   = false;    // última lectura cruda
  static unsigned long tCambio = 0;

  if (lectura != lecturaAnt) {         // la lectura cruda cambió: reinicia timer
    tCambio = millis();
    lecturaAnt = lectura;
  }

  if ((millis() - tCambio) > ANTIRREBOTE_MS && lectura != estadoEstable) {
    estadoEstable = lectura;           // confirma el nuevo estado
    if (estadoEstable) {               // flanco  presente -> INTERRUMPIDO  = 1 objeto
      cuenta = (cuenta + 1) % 10;
      mostrar(cuenta);
      Serial.printf("Objeto detectado.  Cuenta = %d\n", cuenta);
    }
  }
}

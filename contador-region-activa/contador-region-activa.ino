/*
 * ============================================================================
 *  Contador óptico 0–9  ·  MODO ANALÓGICO (transistor en REGIÓN ACTIVA)
 *  Práctica 2 · Optoelectrónica (V3736) · Dr. Rubén Estrada Marmolejo · CUCEI–UDG
 *  Placa: ESP32-S3  ·  Entorno: Arduino
 * ============================================================================
 *
 *  QUÉ HACE
 *  --------
 *  Igual que la versión digital (cuenta 0..9 objetos que cruzan el haz y los
 *  muestra en un display de 7 segmentos de CÁTODO COMÚN), pero AQUÍ la señal
 *  del fototransistor NO viene cuadrada: la leemos con el ADC y decidimos por
 *  SOFTWARE si el haz está presente o interrumpido.
 *
 *  POR QUÉ "REGIÓN ACTIVA"
 *  -----------------------
 *  Con una R_L pequeña (o poca luz) el transistor NO satura: trabaja en la
 *  región activa, donde  V_CE = V_CC - I_C·R_L  e  I_C ∝ irradiancia (ver
 *  Ejercicio 3, diseño de R_L en región activa). Entonces:
 *    · Haz PRESENTE      -> mucha I_C -> V_CE BAJO  -> ADC BAJO.
 *    · Haz INTERRUMPIDO  -> I_C ≈ 0   -> V_CE ≈ V_CC -> ADC ALTO.
 *  El umbral lo pones TÚ en el código (no el hardware). Ventaja didáctica:
 *  puedes ver la señal analógica en el Monitor Serie y calibrar el umbral.
 *
 *  IMPORTANTE — VOLTAJE
 *  --------------------
 *  Alimenta el switch óptico a 3.3 V (NO 5 V): el ADC de la ESP32-S3 mide de
 *  0 a ~3.3 V. Nunca apliques 5 V al pin. R_L sugerida: 2.2 kΩ.
 *
 *  CALIBRACIÓN (hazla una vez)
 *  ---------------------------
 *  1) Sube el programa y abre el Monitor Serie a 115200 baud.
 *  2) Con el haz PRESENTE anota el ADC (valor bajo);
 *     con el haz INTERRUMPIDO anota el ADC (valor alto).
 *  3) Pon UMBRAL_BAJO un poco arriba del "presente" y UMBRAL_ALTO un poco
 *     abajo del "interrumpido" (histéresis: evita conteos falsos).
 * ============================================================================
 */

// ------------------------- Pines -------------------------
// Segmentos del display en orden a, b, c, d, e, f, g
const uint8_t SEG[7] = {4, 5, 6, 7, 15, 16, 17};
const uint8_t PIN_SENSOR = 8;   // colector del fototransistor -> ADC1 (GPIO8)

// -------------- Umbrales con HISTÉRESIS (ADC 0..4095) -----
// Ajusta estos dos números con la calibración del Monitor Serie.
const int UMBRAL_ALTO = 2600;   // ADC por ENCIMA de esto = haz INTERRUMPIDO
const int UMBRAL_BAJO = 1400;   // ADC por DEBAJO de esto = haz PRESENTE
                                // (entre ambos, mantiene el estado anterior)

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

// ------------------------ Estado -------------------------
int  cuenta = 0;
bool interrumpido    = false;   // estado actual (con histéresis)
bool interrumpidoAnt = false;
unsigned long tImpresion = 0;   // para no saturar el Monitor Serie

void mostrar(int d) {
  for (int i = 0; i < 7; i++) digitalWrite(SEG[i], patron[d][i]);
}

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 7; i++) pinMode(SEG[i], OUTPUT);
  analogReadResolution(12);                        // 0..4095
  analogSetPinAttenuation(PIN_SENSOR, ADC_11db);   // rango completo ~0..3.3 V
  mostrar(cuenta);
  Serial.println("Contador optico - MODO ANALOGICO (region activa). Cuenta = 0");
  Serial.println("Calibra: observa el ADC con haz presente vs interrumpido.");
}

void loop() {
  int valor = analogRead(PIN_SENSOR);   // 0..4095

  // --- Comparador con histéresis (dos umbrales) ---
  if (valor > UMBRAL_ALTO)      interrumpido = true;    // se interrumpió el haz
  else if (valor < UMBRAL_BAJO) interrumpido = false;   // el haz volvió
  // (si queda entre UMBRAL_BAJO y UMBRAL_ALTO, no cambia: eso es la histéresis)

  // Cuenta en el flanco  presente -> INTERRUMPIDO
  if (interrumpido && !interrumpidoAnt) {
    cuenta = (cuenta + 1) % 10;
    mostrar(cuenta);
    Serial.printf("Objeto detectado (ADC=%d).  Cuenta = %d\n", valor, cuenta);
  }
  interrumpidoAnt = interrumpido;

  // Traza periódica del valor analógico (para calibrar los umbrales)
  if (millis() - tImpresion > 250) {
    tImpresion = millis();
    Serial.printf("ADC = %4d  | estado: %s\n",
                  valor, interrumpido ? "INTERRUMPIDO" : "presente");
  }
}

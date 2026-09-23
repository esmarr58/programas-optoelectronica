/* ==========================================================================
 *  Practica 3 - Medidor de distancia por IR (reflectancia y transmision directa)
 *  Optoelectronica (V3736) - Dr. Ruben Estrada Marmolejo, CUCEI-UDG.
 *  ESP32-S3 (Arduino). Guia PASO A PASO por el Monitor Serial (115200 bps).
 *
 *  QUE HACE:
 *   - Lee el fototransistor en el COLECTOR (Vout = VCC - Ic*Rc): mas luz -> menor Vout.
 *   - Muestra el valor RAW del ADC, el voltaje SIN CALIBRAR (modelo ideal) y el
 *     voltaje CALIBRADO (usa la calibracion de fabrica del ESP32, analogReadMilliVolts).
 *   - Aplica un MODELO LINEAL: el alumno escribe la PENDIENTE (m) y el OFFSET (b)
 *     que obtuvo en Google Sheets por minimos cuadrados sobre la variable
 *     linealizada X = 1/sqrt(Vsig), con Vsig = VCC - Vout.   d = m*X + b
 *   - Aplica un MODELO CUADRATICO: el alumno escribe A2, A1, A0 que obtuvo con una
 *     TABLA en la hoja de calculo y regresion polinomial.    d = A2*V^2 + A1*V + A0
 *   - Ayuda a CAPTURAR la tabla de calibracion (imprime filas CSV para pegar en Sheets).
 *
 *  Sirve para los DOS metodos de la practica: transmision directa (emisor y
 *  fototransistor enfrentados) y reflectancia (mismo lado, objeto reflector).
 *
 *  Cableado: fototransistor con Rc a VCC; Vout (colector) -> GPIO10 (ADC1_CH9).
 *  Placa (Arduino IDE): "ESP32S3 Dev Module".
 * ========================================================================== */

#include <math.h>

// ---------- Hardware ----------
const int   PIN_SENSOR = 10;      // GPIO10 = ADC1_CH9: Vout del colector
const float VCC        = 3.300f;  // V, alimentacion del divisor (3.3 V del ESP32-S3)
const int   N_PROMEDIO = 16;      // muestras que se promedian por lectura

// ---------- Coeficientes que el ALUMNO obtiene en Google Sheets y PEGA aqui ----------
// Modelo LINEAL sobre la variable linealizada X = 1/sqrt(Vsig):   d[cm] = m*X + b
float PENDIENTE = 2.581f;    // m  (EJEMPLO del deck, 10 medidas; reemplaza con el tuyo)
float OFFSET    = -1.463f;   // b  (EJEMPLO del deck; reemplaza con el tuyo)
// Modelo CUADRATICO (regresion polinomial en la hoja):  d[cm] = coefA*V^2 + coefB*V + coefC
// (NO uses A2/A1/A0: en Arduino son alias de pines analogicos.)
float coefA = 0.0f, coefB = 0.0f, coefC = 0.0f;   // (rellena con los tuyos)

// ---------- Estado de la guia ----------
bool  transmitiendo = false;      // modo "stream" (lectura continua)
unsigned long ultimo = 0;

// ---------- Prototipos ----------
void imprimirGuia();
void imprimirMenu();
void imprimirCoeficientes();

// ---------- Lecturas del ADC ----------
int leerCrudoPromedio() {
  long suma = 0;
  for (int i = 0; i < N_PROMEDIO; i++) suma += analogRead(PIN_SENSOR);
  return (int)(suma / N_PROMEDIO);
}
int milivoltSinCalibrar(int crudo) {   // modelo ideal (multiplicar primero)
  return (int)(3300L * crudo / 4095L);
}
int milivoltCalibrado() {               // calibracion de fabrica del ESP32
  long suma = 0;
  for (int i = 0; i < N_PROMEDIO; i++) suma += analogReadMilliVolts(PIN_SENSOR);
  return (int)(suma / N_PROMEDIO);
}

// ---------- Modelos de distancia ----------
// Variable linealizada: X = 1 / sqrt(Vsig), con Vsig = VCC - Vout (en VOLTS).
float variableLinealizada(float vout_volts) {
  float vsig = VCC - vout_volts;
  if (vsig <= 0.001f) return 0.0f;      // fuera de rango (poca luz / saturado)
  return 1.0f / sqrtf(vsig);
}
float distanciaLineal(float x) {         // d = m*X + b
  return PENDIENTE * x + OFFSET;
}
float distanciaCuadratica(float vout_volts) {   // d = coefA*V^2 + coefB*V + coefC
  return coefA * vout_volts * vout_volts + coefB * vout_volts + coefC;
}

// ---------- Presentacion ----------
void mostrarLectura() {
  int crudo   = leerCrudoPromedio();
  int mvSin   = milivoltSinCalibrar(crudo);
  int mvCal   = milivoltCalibrado();
  float vCal  = mvCal / 1000.0f;
  float x     = variableLinealizada(vCal);
  float dLin  = distanciaLineal(x);
  float dCua  = distanciaCuadratica(vCal);

  Serial.printf("raw=%4d | V_sin_cal=%4d mV | V_cal=%4d mV | Vsig=%.3f V | X=%.3f | d_lineal=%.2f cm | d_cuad=%.2f cm\n",
                crudo, mvSin, mvCal, (VCC - vCal), x, dLin, dCua);
}

void capturarPunto(float distanciaReal) {
  int crudo  = leerCrudoPromedio();
  int mvCal  = milivoltCalibrado();
  float vCal = mvCal / 1000.0f;
  float x    = variableLinealizada(vCal);
  // Fila lista para pegar en Google Sheets (CSV): d_real, raw, V_cal[mV], X
  Serial.printf("PUNTO,%.1f,%d,%d,%.4f\n", distanciaReal, crudo, mvCal, x);
  Serial.println("  ^ copia esta fila (d_real, raw, V_cal_mV, X) a tu hoja de calculo.");
}

void imprimirGuia() {
  Serial.println();
  Serial.println("===== PRACTICA 3: MEDIDOR DE DISTANCIA IR (guia paso a paso) =====");
  Serial.println("PASO 1  Coloca el sensor. Elige el metodo: transmision directa (enfrentados)");
  Serial.println("        o reflectancia (mismo lado, objeto reflector).");
  Serial.println("PASO 2  Escribe 'leer' para una lectura, o 'stream' para lectura continua.");
  Serial.println("        Veras: raw, V sin calibrar, V calibrado, Vsig, X y las distancias.");
  Serial.println("PASO 3  Barrido 0..15 cm. En cada distancia escribe 'punto <cm>' (ej. 'punto 5')");
  Serial.println("        para registrar una fila CSV. Hazlo de SUBIDA (0->15) y de BAJADA (15->0).");
  Serial.println("PASO 4  Pega las filas en Google Sheets. Calcula por minimos cuadrados la recta");
  Serial.println("        X vs d (variable X=1/raiz(Vsig)) -> obten PENDIENTE (m) y OFFSET (b).");
  Serial.println("PASO 5  Ajusta tambien una CUADRATICA d = A2*V^2 + A1*V + A0 (regresion polinomial).");
  Serial.println("PASO 6  Escribe los coeficientes en el codigo (PENDIENTE/OFFSET y A2/A1/A0) y vuelve");
  Serial.println("        a subir el programa: ahora 'leer' mostrara la distancia estimada por ambos modelos.");
  imprimirMenu();
}

void imprimirMenu() {
  Serial.println("--- comandos: 'leer' | 'stream' (on/off) | 'punto <cm>' | 'coef' | 'ayuda' ---");
}

void imprimirCoeficientes() {
  Serial.printf("Modelo LINEAL:     d = %.4f * X + (%.4f)   [X = 1/sqrt(VCC - Vout)]\n", PENDIENTE, OFFSET);
  Serial.printf("Modelo CUADRATICO: d = %.5f*V^2 + %.5f*V + (%.5f)\n", coefA, coefB, coefC);
}

// ---------- Procesa un comando del usuario ----------
void procesarComando(String linea) {
  linea.trim();
  if (linea.equalsIgnoreCase("leer"))       { mostrarLectura(); }
  else if (linea.equalsIgnoreCase("stream")){ transmitiendo = !transmitiendo;
      Serial.println(transmitiendo ? "stream ON (escribe 'stream' para detener)" : "stream OFF"); }
  else if (linea.equalsIgnoreCase("coef"))  { imprimirCoeficientes(); }
  else if (linea.equalsIgnoreCase("ayuda")) { imprimirGuia(); }
  else if (linea.startsWith("punto"))       {
      float d = linea.substring(5).toFloat();
      capturarPunto(d);
  }
  else if (linea.length() > 0) { Serial.println("Comando no reconocido. Escribe 'ayuda'."); }
}

// ---------- Arduino ----------
void setup() {
  Serial.begin(115200);
  delay(300);
  analogReadResolution(12);                 // raw 0..4095
  analogSetAttenuation(ADC_11db);           // rango ~0..3.1-3.3 V (12 dB)
  imprimirGuia();
  imprimirCoeficientes();
}

void loop() {
  if (Serial.available()) {
    String linea = Serial.readStringUntil('\n');
    procesarComando(linea);
  }
  if (transmitiendo && millis() - ultimo > 500) {
    ultimo = millis();
    mostrarLectura();
  }
}

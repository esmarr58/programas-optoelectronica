/* ==========================================================================
 *  Practica 4 - Convertidores de nivel y control de motor con PC817 + ESP32-S3
 *  Optoelectronica (V3736) - Dr. Ruben Estrada Marmolejo, CUCEI-UDG.
 *  ESP32-S3 (Arduino). Guia PASO A PASO por el Monitor Serial (115200 bps).
 *
 *  REQUISITOS DE SOFTWARE:
 *    - Arduino IDE con el core "Arduino-ESP32" v3.x (API LEDC nueva: ledcAttach).
 *    - Placa: "ESP32S3 Dev Module".
 *
 *  QUE HACE (los tres circuitos de la practica, todos aislados con PC817):
 *
 *   CIRCUITO 1 - Convertidor de nivel 3.3 V -> 24 V (SALIDA aislada)
 *     El GPIO del ESP32-S3 enciende el LED del PC817 (via R_in = 220 ohm).
 *     El fototransistor, con pull-up a 24 V, conmuta una senal de 24 V.
 *     OJO: logica INVERTIDA -> LED ON => salida (colector) BAJA (~0.2 V).
 *
 *   CIRCUITO 2 - Convertidor de nivel 24 V -> 3.3 V (ENTRADA aislada)
 *     Una senal de 24 V excita el LED del PC817 (via R_in = 4.7k).
 *     El fototransistor, con pull-up a 3.3 V, entrega un nivel que el GPIO lee.
 *     OJO: logica INVERTIDA -> 24 V presente => el GPIO lee BAJO (0).
 *     Por eso aqui se reporta la senal YA corregida (negada en software).
 *
 *   CIRCUITO 3 - Control PWM de un motorreductor de 5 V (TT, llanta amarilla)
 *     El ESP32-S3 genera PWM; el PC817 AISLA esa senal de PWM.
 *     El PC817 NO mueve el motor (max 50 mA): su salida ataca un MOSFET de
 *     potencia (canal N, logico) que conmuta el motor de 5 V, con DIODO de
 *     recirculacion (flyback) en paralelo con el motor.
 *     PWM a BAJA frecuencia (~1 kHz) porque el PC817 es LENTO (t_r+t_f ~ 7 us).
 *     OJO: el PC817 + MOSFET (pull-up) INVIERTEN el sentido -> este programa ya
 *     compensa la inversion para que "velocidad 0..100%" sea intuitiva.
 *
 *  CABLEADO (resumen; ver esquematicos de la guia de la practica):
 *    - C1: GPIO4  -> R_in(220) -> LED PC817(1,2);  salida 24V: Rc(10k) a +24V,
 *          colector(4) = salida, emisor(3) a GND_24V.
 *    - C2: +24V senal -> R_in(4.7k) -> LED PC817(1,2);  salida 3.3V: Rc(10k) a
 *          +3.3V, colector(4) -> GPIO5, emisor(3) a GND_ESP.
 *    - C3: GPIO6 -> R_in(220) -> LED PC817(1,2);  lado motor: Rc(10k) de +5V a
 *          colector(4) y a la COMPUERTA del MOSFET; emisor(3) a GND_motor;
 *          MOSFET: drain -> motor(-), source -> GND_motor, motor(+) -> +5V;
 *          diodo flyback: catodo a +5V, anodo al drain.
 *    - IMPORTANTE (aislamiento): cada lado tiene su propia tierra. Si unes
 *          GND_ESP con GND_24V o GND_motor, PIERDES el aislamiento galvanico.
 *
 *  COMANDOS por el Monitor Serial (115200 bps, "Nueva linea"):
 *    ayuda            -> reimprime esta guia
 *    c1 on / c1 off   -> enciende/apaga el LED del convertidor 3.3->24 V
 *    c2               -> lee el convertidor 24->3.3 V (muestra crudo y corregido)
 *    vel <0..100>     -> fija la velocidad del motor en % (circuito 3)
 *    stop             -> motor a 0 %
 *    rampa            -> demo: sube y baja la velocidad del motor
 *    estado           -> imprime el estado de los tres circuitos
 * ========================================================================== */

#include <Arduino.h>

// ----------------------- Pines (ESP32-S3) -----------------------
const int PIN_C1_LED = 4;    // Circuito 1: GPIO -> LED PC817 (salida 3.3->24)
const int PIN_C2_IN  = 5;    // Circuito 2: lee la salida del PC817 (entrada 24->3.3)
const int PIN_C3_PWM = 6;    // Circuito 3: PWM -> LED PC817 -> MOSFET -> motor

// ----------------------- Parametros del PWM del motor -----------------------
const int   PWM_FREQ_HZ = 1000;   // 1 kHz: BAJO a proposito (PC817 es lento)
const int   PWM_RES_BITS = 10;    // 10 bits -> duty 0..1023
const int   PWM_MAX = (1 << PWM_RES_BITS) - 1;

// El PC817 (salida con pull-up) + MOSFET INVIERTEN la senal:
//   duty alto en el LED  => fototransistor conduce => compuerta BAJA => MOSFET OFF
// Con MOTOR_INVERTIDO = true, el programa manda (PWM_MAX - duty) al pin para que
// "velocidad %" sea intuitiva (0 % = motor parado, 100 % = maxima velocidad).
const bool  MOTOR_INVERTIDO = true;

// ----------------------- Estado -----------------------
int   velocidadPct = 0;       // velocidad del motor en %
bool  c1Encendido  = false;   // estado del LED del convertidor 3.3->24

// ----------------------- Prototipos -----------------------
void imprimirGuia();
void aplicarVelocidad(int pct);
int  leerConvertidor2();
void imprimirEstado();
void procesarComando(String cmd);
void demoRampa();

// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(300);

  // Circuito 1: salida digital hacia el LED del PC817
  pinMode(PIN_C1_LED, OUTPUT);
  digitalWrite(PIN_C1_LED, LOW);   // LED apagado al inicio

  // Circuito 2: entrada con pull-up interno (respalda al Rc externo)
  pinMode(PIN_C2_IN, INPUT_PULLUP);

  // Circuito 3: PWM con la API LEDC nueva (core Arduino-ESP32 v3.x)
  // ledcAttach(pin, frecuencia, resolucion) configura el canal automaticamente.
  if (!ledcAttach(PIN_C3_PWM, PWM_FREQ_HZ, PWM_RES_BITS)) {
    Serial.println("ERROR: no se pudo configurar el PWM (LEDC). Revisa el core v3.x.");
  }
  aplicarVelocidad(0);             // motor parado

  imprimirGuia();
  imprimirEstado();
}

// =====================================================================
void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) procesarComando(cmd);
  }
}

// =====================================================================
// Convierte "velocidad %" en el valor de duty que se escribe al pin,
// compensando la inversion del PC817 + MOSFET si hace falta.
void aplicarVelocidad(int pct) {
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;
  velocidadPct = pct;

  int dutyDeseado = (int)((long)pct * PWM_MAX / 100);   // duty que "ve" el motor
  int dutyPin = MOTOR_INVERTIDO ? (PWM_MAX - dutyDeseado) : dutyDeseado;
  ledcWrite(PIN_C3_PWM, dutyPin);
}

// Lee el convertidor 24->3.3 V. Devuelve 1 si hay 24 V presentes, 0 si no.
// El GPIO lee BAJO cuando hay 24 V (logica invertida) -> se niega aqui.
int leerConvertidor2() {
  int crudo = digitalRead(PIN_C2_IN);   // 0 = hay 24 V ; 1 = no hay 24 V
  int corregido = (crudo == LOW) ? 1 : 0;
  return corregido;
}

// =====================================================================
void procesarComando(String cmd) {
  cmd.toLowerCase();

  if (cmd == "ayuda" || cmd == "help" || cmd == "?") {
    imprimirGuia();

  } else if (cmd == "c1 on") {
    c1Encendido = true;
    digitalWrite(PIN_C1_LED, HIGH);      // LED ON -> salida 24V BAJA (invertida)
    Serial.println(">> C1: LED ON. La salida de 24 V deberia estar en ~0.2 V (BAJO).");
    Serial.println("   (Logica invertida: LED encendido = salida baja.)");

  } else if (cmd == "c1 off") {
    c1Encendido = false;
    digitalWrite(PIN_C1_LED, LOW);       // LED OFF -> salida 24V ALTA
    Serial.println(">> C1: LED OFF. La salida de 24 V deberia estar en ~24 V (ALTO).");

  } else if (cmd == "c2") {
    int crudo = digitalRead(PIN_C2_IN);
    int val   = leerConvertidor2();
    Serial.print(">> C2: lectura CRUDA del GPIO = "); Serial.print(crudo);
    Serial.print(" (0=hay 24V por logica invertida). Senal CORREGIDA = ");
    Serial.print(val);
    Serial.println(val ? "  -> HAY 24 V en la entrada." : "  -> NO hay 24 V.");

  } else if (cmd.startsWith("vel")) {
    int pct = cmd.substring(3).toInt();
    aplicarVelocidad(pct);
    Serial.print(">> C3: velocidad del motor = "); Serial.print(velocidadPct);
    Serial.println(" %");

  } else if (cmd == "stop") {
    aplicarVelocidad(0);
    Serial.println(">> C3: motor DETENIDO (0 %).");

  } else if (cmd == "rampa") {
    demoRampa();

  } else if (cmd == "estado") {
    imprimirEstado();

  } else {
    Serial.print("Comando no reconocido: '"); Serial.print(cmd);
    Serial.println("'. Escribe 'ayuda'.");
  }
}

// =====================================================================
void demoRampa() {
  Serial.println(">> C3: rampa de velocidad 0 -> 100 -> 0 % (observa el motor y mide el PWM).");
  for (int p = 0; p <= 100; p += 10) { aplicarVelocidad(p);
    Serial.print("   vel = "); Serial.print(p); Serial.println(" %"); delay(600); }
  for (int p = 100; p >= 0; p -= 10) { aplicarVelocidad(p);
    Serial.print("   vel = "); Serial.print(p); Serial.println(" %"); delay(600); }
  Serial.println(">> Fin de la rampa. Motor en 0 %.");
}

// =====================================================================
void imprimirEstado() {
  Serial.println("------------------------------------------------------------");
  Serial.print("  C1 (3.3->24 V): LED ");
  Serial.print(c1Encendido ? "ON  -> salida 24V BAJA" : "OFF -> salida 24V ALTA");
  Serial.println();
  Serial.print("  C2 (24->3.3 V): entrada de 24 V ");
  Serial.println(leerConvertidor2() ? "PRESENTE" : "ausente");
  Serial.print("  C3 (motor PWM): velocidad = "); Serial.print(velocidadPct);
  Serial.print(" %   (PWM "); Serial.print(PWM_FREQ_HZ); Serial.println(" Hz)");
  Serial.println("------------------------------------------------------------");
}

// =====================================================================
void imprimirGuia() {
  Serial.println();
  Serial.println("==================================================================");
  Serial.println(" PRACTICA 4 - PC817 + ESP32-S3  (Optoelectronica V3736)");
  Serial.println(" Convertidores de nivel 3.3<->24 V y control PWM de motor aislado");
  Serial.println("==================================================================");
  Serial.println(" Recuerda: cada lado tiene su PROPIA tierra. No unas las masas o");
  Serial.println(" pierdes el aislamiento galvanico que da el PC817.");
  Serial.println(" El PC817 es LENTO: el PWM del motor va a 1 kHz (no a 20 kHz).");
  Serial.println(" La logica de los convertidores sale INVERTIDA (se corrige aqui).");
  Serial.println(" COMANDOS:");
  Serial.println("   ayuda            - reimprime esta guia");
  Serial.println("   c1 on | c1 off   - LED del convertidor 3.3 -> 24 V");
  Serial.println("   c2               - lee el convertidor 24 -> 3.3 V");
  Serial.println("   vel <0..100>     - velocidad del motor en %");
  Serial.println("   stop             - motor a 0 %");
  Serial.println("   rampa            - demo de velocidad variable");
  Serial.println("   estado           - estado de los tres circuitos");
  Serial.println("==================================================================");
}

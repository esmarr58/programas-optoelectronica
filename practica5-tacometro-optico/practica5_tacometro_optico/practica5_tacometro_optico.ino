/* Practica 5 v3: TCRT + RC directo, IRQ FALLING. Optoelectronica 2026B, CUCEI-UDG.
 * Arduino-ESP32 3.x, ESP32S3 Dev Module; Monitor Serial 115200, nueva linea.
 * Reutiliza C3 de P4: GPIO6 -> 220 ohm -> PC817(1A,2K a GND_ESP).
 * PC817(4C) -> gate IRLZ44N y pull-up 10k a +5V; (3E) -> GND_motor.
 * MOSFET 1G/2D/3S (frente marcado): D motor(-), S GND_motor.
 * Flyback: anodo D, catodo +5V; motor(+) +5V. Tierras separadas.
 * Agrega TCRT5000 discreto: LED A por 220 ohm a3V3, K aGND_ESP;
 * transistor C -> nodoA (pull-up10k a3V3,10nF aGND_ESP,GPIO10ADC diagnostico).
 * NodoA -> GPIO4 INPUT IRQ FALLING y GPIO10 ADC solo diagnostico10Hz.
 * Un solo CF10nF entre C y E; no comparador ni resistencia serie1k.
 * Filtro software = tiempo muerto entre flancos aceptados, NO histeresis.
 * GPIOHIGH apaga motor solo cuando el ESP ejecuta y maneja la salida.
 * ADVERTENCIA: pull-up en gate puede encender motor durante reset/perdidaESP.
 * Conectar fuente motor al final; desconectarla antes de reset/carga/cableado.
 * Sin lazo cerrado: PWM solicitado (%) y RPM medidos son magnitudes distintas.
 */
#include <Arduino.h>
#include <stdarg.h>
#include <string.h>
#include "tacometro.h"

constexpr uint8_t PIN_PWM = 6, PIN_SENSOR = 10, PIN_ENCODER = 4, PIN_MARCA = 5;
constexpr uint32_t PWM_HZ = 1000, DIAGNOSTICO_MS = 100, VENTANA_MS = 1000;
constexpr uint32_t FILTRO_INICIAL_US = 500;
constexpr uint8_t PWM_BITS = 10;
constexpr uint16_t PWM_MAX = (1U << PWM_BITS) - 1;
constexpr uint32_t PASO_RAMPA_MS = 3000;
uint16_t ppr = 4;
uint8_t pwmPct = 0;
bool pwmListo = false, streamActivo = false, rampaActiva = false;
int pasoRampa = 0;
portMUX_TYPE muxConteo = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t inicioVentana = 0, pulsosVentana = 0, pulsosTotal = 0;
volatile uint32_t crudosTotal = 0, ultimoPulsoUs = 0, filtroUs = FILTRO_INICIAL_US;
volatile bool huboPulso = false;
volatile bool nivelMarca = false;
uint32_t ultimoDiagnosticoMs = 0, ultimaRampaMs = 0;
uint32_t sensorMv = 0, mensajesOmitidos = 0;
struct EstadoConteo {
  uint32_t ahoraMs, inicioMs, pulsos, total, crudos, rechazados, filtro;
};
char comando[96];
size_t longitudComando = 0;
bool descartandoLinea = false;
// Cola TX: Serial.print puede esperar; se vacia solo lo que acepta el puerto.
char colaTx[2048];
size_t txInicio = 0, txFin = 0;

void decir(const char *formato, ...) {
  char linea[512];
  va_list args;
  va_start(args, formato);
  const int n = vsnprintf(linea, sizeof(linea), formato, args);
  va_end(args);
  if (n < 0 || size_t(n) >= sizeof(linea)) { ++mensajesOmitidos; return; }
  const size_t libres = (txInicio + sizeof(colaTx) - txFin - 1) % sizeof(colaTx);
  if (size_t(n) > libres) { ++mensajesOmitidos; return; }
  for (int i = 0; i < n; ++i) {
    colaTx[txFin] = linea[i];
    txFin = (txFin + 1) % sizeof(colaTx);
  }
}
void servirTx() {
  int disponibles = Serial.availableForWrite();
  for (int n = 0; n < 64 && disponibles-- > 0 && txInicio != txFin; ++n) {
    Serial.write(uint8_t(colaTx[txInicio]));
    txInicio = (txInicio + 1) % sizeof(colaTx);
  }
}
void ARDUINO_ISR_ATTR contarPulso() {
  // No ADC, Serial, float ni asignacion dinamica. La misma mux en ISR y tarea.
  portENTER_CRITICAL_ISR(&muxConteo);
  const uint32_t ahoraUs = micros();
  ++crudosTotal; // flancos FALLING atendidos por ISR, incluidos los rechazados
  if (!huboPulso || filtroUs == 0 || uint32_t(ahoraUs - ultimoPulsoUs) >= filtroUs) {
    ultimoPulsoUs = ahoraUs;
    huboPulso = true;
    ++pulsosVentana;
    ++pulsosTotal;
    nivelMarca = !nivelMarca;
    digitalWrite(PIN_MARCA, nivelMarca ? HIGH : LOW); // cada CAMBIO = un aceptado
  }
  portEXIT_CRITICAL_ISR(&muxConteo);
}
EstadoConteo leerConteo() {
  portENTER_CRITICAL(&muxConteo);
  const EstadoConteo e{millis(), inicioVentana, pulsosVentana, pulsosTotal,
    crudosTotal, uint32_t(crudosTotal - pulsosTotal), filtroUs};
  portEXIT_CRITICAL(&muxConteo);
  return e;
}
void configurarFiltro(uint32_t us) {
  portENTER_CRITICAL(&muxConteo);
  filtroUs = us;
  inicioVentana = millis();
  pulsosVentana = 0;
  // Conservar ultimo aceptado/huboPulso: no habilitar un primer pulso artificial.
  portEXIT_CRITICAL(&muxConteo);
}
void reiniciarMedicion() {
  portENTER_CRITICAL(&muxConteo);
  inicioVentana = millis();
  pulsosVentana = 0;
  // No resetear acumulados ni ultimo flanco: el filtro cruza entre ventanas.
  portEXIT_CRITICAL(&muxConteo);
}
bool tomarVentana(EstadoConteo &e) {
  portENTER_CRITICAL(&muxConteo);
  const uint32_t ahora = millis();
  if (uint32_t(ahora - inicioVentana) < VENTANA_MS) {
    portEXIT_CRITICAL(&muxConteo);
    return false;
  }
  e = EstadoConteo{ahora, inicioVentana, pulsosVentana, pulsosTotal,
    crudosTotal, uint32_t(crudosTotal - pulsosTotal), filtroUs};
  pulsosVentana = 0;
  inicioVentana = ahora;
  portEXIT_CRITICAL(&muxConteo);
  return true;
}
bool aplicarPWM(uint8_t pct) {
  if (!pwmListo) { decir("# ERROR PWM no disponible; desconecte fuente motor.\n"); return false; }
  const uint32_t dutyPin = PWM_MAX - (uint32_t(pct) * PWM_MAX / 100);
  if (!ledcWrite(PIN_PWM, dutyPin)) {
    pwmListo = false;
    rampaActiva = false;
    ledcDetach(PIN_PWM);
    digitalWrite(PIN_PWM, HIGH);
    pinMode(PIN_PWM, OUTPUT);
    decir("# ERROR ledcWrite; salida HIGH solicitada; desconecte fuente motor.\n");
    return false;
  }
  if (pct != pwmPct) { pwmPct = pct; reiniciarMedicion(); }
  return true;
}
void imprimirAyuda() {
  decir("# P5 tacometro: PWM solicitado no equivale a velocidad real.\n"
        "# ayuda | vel 0..100 | stop | estado | leer | stream on/off\n"
        "# ppr 1..100 | filtro 0..100000 (us; solo PWM0 y rampa off) | rampa\n"
        "# Rampa 0->100->0, pasos10%% cada3000ms; stop/vel la cancelan.\n"
        "# TCRT RC directo: GPIO4 FALLING; GPIO10 ADC diagnostico10Hz.\n"
        "# Tiempo muerto inicial500us, NO histeresis ni medida de ancho de pulso.\n");
}
void imprimirEstado() {
  const EstadoConteo e = leerConteo();
  decir("# estado tiempo_ms=%lu pwm_pct=%u pwm_ok=%u mV=%lu irq_gpio4=%d ppr=%u rampa=%u stream=%u\n",
    (unsigned long)e.ahoraMs, pwmPct, pwmListo, (unsigned long)sensorMv, digitalRead(PIN_ENCODER), ppr, rampaActiva, streamActivo);
  decir("# ventana parcial: pulsos=%lu dt_ms=%lu pulsos_total=%lu crudos_total=%lu rechazados_total=%lu filtro_us=%lu tx_omitidos=%lu\n",
    (unsigned long)e.pulsos, (unsigned long)(e.ahoraMs - e.inicioMs), (unsigned long)e.total,
    (unsigned long)e.crudos, (unsigned long)e.rechazados, (unsigned long)e.filtro, (unsigned long)mensajesOmitidos);
}
void procesarComando(char *linea) {
  char *tokens[5];
  uint8_t cantidad = 0;
  char *contexto;
  for (char *t = strtok_r(linea, " \t", &contexto); t; t = strtok_r(nullptr, " \t", &contexto)) {
    if (cantidad == 5) { decir("# ERROR demasiados argumentos.\n"); return; }
    tokens[cantidad++] = t;
  }
  if (!cantidad) return;
  uint32_t a = 0;
  if (cantidad == 1 && !strcmp(tokens[0], "ayuda")) imprimirAyuda();
  else if (cantidad == 1 && !strcmp(tokens[0], "estado")) imprimirEstado();
  else if (cantidad == 1 && !strcmp(tokens[0], "leer")) {
    const EstadoConteo e = leerConteo();
    decir("# leer tiempo_ms=%lu mV=%lu irq_gpio4=%d pulsos_total=%lu crudos_total=%lu rechazados_total=%lu filtro_us=%lu (ADC10Hz).\n",
      (unsigned long)e.ahoraMs, (unsigned long)sensorMv, digitalRead(PIN_ENCODER), (unsigned long)e.total,
      (unsigned long)e.crudos, (unsigned long)e.rechazados, (unsigned long)e.filtro);
  }
  else if (cantidad == 1 && !strcmp(tokens[0], "stop")) {
    rampaActiva = false;
    if (aplicarPWM(0)) decir("# PWM solicitado=0%%; motor puede seguir girando por inercia.\n");
  } else if (cantidad == 2 && !strcmp(tokens[0], "vel") && enteroEstricto(tokens[1], 100, a)) {
    rampaActiva = false;
    if (aplicarPWM(uint8_t(a))) decir("# PWM solicitado=%lu%%.\n", (unsigned long)a);
  } else if (cantidad == 2 && !strcmp(tokens[0], "ppr") && enteroEstricto(tokens[1], 100, a) && a >= 1) {
    ppr = uint16_t(a); reiniciarMedicion();
    decir("# PPR=%u; ventana reiniciada atomicamente; acumulado conservado.\n", ppr);
  } else if (!strcmp(tokens[0], "umbral")) {
    decir("# ERROR umbral no existe: GPIO4 FALLING directo; no hay histeresis ajustable.\n");
  } else if (cantidad == 2 && !strcmp(tokens[0], "filtro") && enteroEstricto(tokens[1], 100000, a)) {
    if (pwmPct != 0 || rampaActiva) decir("# ERROR filtro: primero stop y esperar reposo fisico.\n");
    else {
      configurarFiltro(a);
      decir("# Tiempo muerto=%luus; ventana reiniciada; totales y ultimo aceptado conservados.\n", (unsigned long)a);
    }
  } else if (cantidad == 2 && !strcmp(tokens[0], "stream") &&
    (!strcmp(tokens[1], "on") || !strcmp(tokens[1], "off"))) {
    streamActivo = !strcmp(tokens[1], "on");
    if (streamActivo) decir("tiempo_ms,pwm_pct,mV,pulsos,ppr,dt_ms,rpm\n");
    else decir("# Stream off.\n");
  } else if (cantidad == 1 && !strcmp(tokens[0], "rampa")) {
    rampaActiva = false;
    if (aplicarPWM(0)) {
      rampaActiva = true; pasoRampa = 0; ultimaRampaMs = millis();
      reiniciarMedicion(); decir("# Rampa iniciada: PWM=0%%; 3s por paso.\n");
    }
  } else decir("# ERROR sintaxis/rango; escriba ayuda. No se modifico el estado.\n");
}
void servirSerial() {
  // Presupuesto por vuelta: ni una linea incompleta ni un flujo grande bloquean.
  for (int i = 0; i < 32 && Serial.available(); ++i) {
    const char c = char(Serial.read());
    if (c == '\r') continue;
    if (c == '\n') {
      if (descartandoLinea) decir("# ERROR linea larga/no ASCII; descartada completa.\n");
      else { comando[longitudComando] = '\0'; procesarComando(comando); }
      longitudComando = 0; descartandoLinea = false;
    } else if (!descartandoLinea) {
      if ((c < 32 && c != '\t') || c > 126 || longitudComando >= sizeof(comando) - 1)
        descartandoLinea = true;
      else comando[longitudComando++] = c;
    }
  }
}
void actualizarDiagnostico() {
  const uint32_t ahora = millis();
  if (uint32_t(ahora - ultimoDiagnosticoMs) < DIAGNOSTICO_MS) return;
  ultimoDiagnosticoMs = ahora;
  sensorMv = analogReadMilliVolts(PIN_SENSOR);
}
void cerrarVentana() {
  EstadoConteo e;
  if (!tomarVentana(e)) return;
  const uint32_t dt = e.ahoraMs - e.inicioMs;
  if (streamActivo) decir("%lu,%u,%lu,%lu,%u,%lu,%.2f\n", (unsigned long)e.ahoraMs, pwmPct,
    (unsigned long)sensorMv, (unsigned long)e.pulsos, ppr, (unsigned long)dt, calcularRPM(e.pulsos, ppr, dt));
}
void actualizarRampa() {
  if (!rampaActiva || uint32_t(millis() - ultimaRampaMs) < PASO_RAMPA_MS) return;
  ultimaRampaMs = millis(); // si se demora, no saltar varios pasos de golpe
  if (++pasoRampa > 20) {
    rampaActiva = false; decir("# Fin de rampa: PWM solicitado=0%%.\n"); return;
  }
  const uint8_t pct = uint8_t(pasoRampa <= 10 ? pasoRampa * 10 : (20 - pasoRampa) * 10);
  if (aplicarPWM(pct)) decir("# Rampa: PWM solicitado=%u%%.\n", pct);
}
void setup() {
  // Lo primero, aun antes del puerto serie. No protege el intervalo de reset.
  digitalWrite(PIN_PWM, HIGH);
  pinMode(PIN_PWM, OUTPUT);
  digitalWrite(PIN_MARCA, LOW);
  pinMode(PIN_MARCA, OUTPUT);
  Serial.begin(115200);
  pwmListo = ledcAttach(PIN_PWM, PWM_HZ, PWM_BITS);
  if (pwmListo) aplicarPWM(0);
  else {
    digitalWrite(PIN_PWM, HIGH); pinMode(PIN_PWM, OUTPUT);
    decir("# ERROR ledcAttach: desconecte fuente motor; revise core3.x.\n");
  }
  pinMode(PIN_SENSOR, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_SENSOR, ADC_11db);
  pinMode(PIN_ENCODER, INPUT); // nodoA con pullup externo10k3V3, sin pullup interno
  ultimoDiagnosticoMs = millis();
  reiniciarMedicion();
  attachInterrupt(digitalPinToInterrupt(PIN_ENCODER), contarPulso, FALLING);
  decir("# Fuente motor al final. Desconectar antes de reset/carga: pullup puede ACTIVAR motor.\n");
  imprimirAyuda();
}
void loop() {
  actualizarDiagnostico(); // ADC no participa en el conteo de RPM
  cerrarVentana(); // cerrar la consigna anterior antes de cambiar PWM
  servirSerial();
  actualizarRampa();
  servirTx();
}

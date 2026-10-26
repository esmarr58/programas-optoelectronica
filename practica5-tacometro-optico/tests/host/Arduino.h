#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string>
#include <deque>
#include <atomic>
#include <mutex>
#include <assert.h>
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
#define ADC_11db 3
#define strtok_r strtok_s
#define ARDUINO_ISR_ATTR
#define RISING 1
#define FALLING 2
using portMUX_TYPE = std::mutex;
#define portMUX_INITIALIZER_UNLOCKED {}
inline thread_local int profundidadCritica = 0;
inline void portENTER_CRITICAL(portMUX_TYPE *m) { m->lock(); ++profundidadCritica; }
inline void portEXIT_CRITICAL(portMUX_TYPE *m) { --profundidadCritica; m->unlock(); }
inline void portENTER_CRITICAL_ISR(portMUX_TYPE *m) { portENTER_CRITICAL(m); }
inline void portEXIT_CRITICAL_ISR(portMUX_TYPE *m) { portEXIT_CRITICAL(m); }
inline std::atomic<uint32_t> relojMs{0}, relojUs{0};
inline uint32_t adcMv = 0, dutyEscrito = 0, lecturasADC = 0;
inline void (*isrRegistrada)() = nullptr;
inline int modoIRQ = 0, pinIRQ = -1;
inline bool attachOk = true, writeOk = true, detached = false;
inline int nivelPin = LOW;
inline int nivelMarcaHost = LOW;
inline std::atomic<uint32_t> cambiosMarca{0};
inline uint32_t millis() { return relojMs; }
inline uint32_t micros() { return relojUs; }
inline int digitalRead(uint8_t) { return 0; }
inline int digitalPinToInterrupt(uint8_t p) { return p; }
inline void attachInterrupt(int p, void (*isr)(), int modo) { pinIRQ=p; isrRegistrada=isr; modoIRQ=modo; }
inline void digitalWrite(uint8_t p, int n) {
  if (p == 6) nivelPin = n;
  if (p == 5) { if (n != nivelMarcaHost) ++cambiosMarca; nivelMarcaHost = n; }
}
inline void pinMode(uint8_t, int) {}
inline bool ledcAttach(uint8_t, uint32_t, uint8_t) { return attachOk; }
inline bool ledcWrite(uint8_t, uint32_t duty) { dutyEscrito = duty; return writeOk; }
inline bool ledcDetach(uint8_t) { detached = true; return true; }
inline void analogReadResolution(int) {}
inline void analogSetPinAttenuation(uint8_t, int) {}
inline uint32_t analogReadMilliVolts(uint8_t) { ++lecturasADC; return adcMv; }
struct PuertoSimulado {
  std::string salida;
  std::deque<char> entrada;
  void begin(int) {}
  int availableForWrite() { return 256; }
  int available() { return int(entrada.size()); }
  int read() { char c = entrada.front(); entrada.pop_front(); return uint8_t(c); }
  size_t write(uint8_t c) { salida += char(c); return 1; }
} inline Serial;

#pragma once
#include <stdint.h>

// Utilidades sin Arduino. El conteo por interrupcion vive en el sketch.
inline double calcularRPM(uint32_t pulsos, uint16_t ppr, uint32_t dtMs) {
  return (ppr && dtMs) ? 60000.0 * pulsos / (double(ppr) * dtMs) : 0.0;
}
// Solo digitos, sin signo, basura final ni desbordamientos.
inline bool enteroEstricto(const char *s, uint32_t maximo, uint32_t &valor) {
  if (!s || !*s) return false;
  uint32_t n = 0;
  for (; *s; ++s) {
    if (*s < '0' || *s > '9') return false;
    const uint32_t digito = uint32_t(*s - '0');
    if (digito > maximo || n > (maximo - digito) / 10) return false;
    n = n * 10 + digito;
  }
  valor = n;
  return true;
}

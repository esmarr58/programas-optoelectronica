#include <assert.h>
#include <math.h>
#include <string.h>
#include <thread>
#include "../../practica5_tacometro_optico.ino"
void comandoHost(const char *s) { char b[96]; strcpy_s(b, s); procesarComando(b); }
void drenar() { while (txInicio != txFin) servirTx(); }
void flanco(uint32_t us) { relojUs=us; isrRegistrada(); }
int main() {
  setup();
  assert(nivelPin==HIGH && dutyEscrito==1023 && pwmPct==0);
  assert(isrRegistrada==contarPulso && pinIRQ==4 && modoIRQ==FALLING && nivelMarcaHost==LOW);
  imprimirEstado(); drenar(); assert(Serial.salida.find("# estado tiempo_ms=0")!=std::string::npos);
  assert(leerConteo().total==0); // registrar ISR no inventa pulso
  flanco(10); assert(leerConteo().total==1); // primer flanco aceptado sin espera
  flanco(509); assert(leerConteo().rechazados==1 && leerConteo().total==1);
  flanco(510); assert(leerConteo().total==2); // exactamente 500us
  assert(leerConteo().crudos==3 && cambiosMarca==2 && nivelMarcaHost==LOW);
  assert(fabs(calcularRPM(20,4,1000)-300)<0.001);
  assert(fabs(calcularRPM(20,4,1250)-240)<0.001);
  assert(fabs(calcularRPM(1,4,1000)-15)<0.001);
  assert(calcularRPM(0,4,1000)==0);
  uint32_t n;
  assert(enteroEstricto("100",100,n) && n==100);
  for (const char *s : {"", "-1", "+1", "101", "20abc", "1.0", "999999999999999999999"})
    assert(!enteroEstricto(s,100,n));
  // Snapshot+reset continuo; filtro no se reinicia al cerrar ventana.
  relojMs=0; reiniciarMedicion(); flanco(1010);
  streamActivo=true; relojMs=1250; cerrarVentana();
  assert(leerConteo().pulsos==0);
  flanco(1200); assert(leerConteo().pulsos==0);
  flanco(1510); assert(leerConteo().pulsos==1);
  drenar(); assert(Serial.salida.find(",4,1250,12.00\n")!=std::string::npos);
  // Rollover de micros y millis conservando las restas modulares.
  portENTER_CRITICAL(&muxConteo);
  ultimoPulsoUs=UINT32_MAX-249; huboPulso=true;
  inicioVentana=UINT32_MAX-499; pulsosVentana=4;
  portEXIT_CRITICAL(&muxConteo);
  flanco(249); const auto previo=leerConteo();
  flanco(250); assert(leerConteo().total==previo.total+1);
  relojMs=500; EstadoConteo ventana; assert(tomarVentana(ventana));
  assert(uint32_t(ventana.ahoraMs-ventana.inicioMs)==1000 && ventana.pulsos==5);
  // ADC no cuenta: se actualiza a10Hz y puede permanecer fijo.
  const auto antesADC=leerConteo(); adcMv=3000;
  relojMs=ultimoDiagnosticoMs+99; actualizarDiagnostico(); assert(lecturasADC==0);
  relojMs=ultimoDiagnosticoMs+100; actualizarDiagnostico(); assert(lecturasADC==1);
  assert(leerConteo().total==antesADC.total);
  comandoHost("vel 50"); assert(pwmPct==50 && dutyEscrito==512);
  for (const char *s : {"vel -1","vel abc","vel 101","vel 20 extra","velocidad 40","stop extra"}) {
    comandoHost(s); assert(pwmPct==50);
  }
  flanco(1000); const auto antesPPR=leerConteo();
  comandoHost("ppr 0"); assert(ppr==4 && leerConteo().pulsos==antesPPR.pulsos);
  comandoHost("ppr 8"); assert(ppr==8 && leerConteo().pulsos==0 && leerConteo().total==antesPPR.total);
  comandoHost("umbral 1200 2000"); assert(ppr==8 && leerConteo().total==antesPPR.total);
  drenar(); assert(Serial.salida.find("ERROR umbral no existe")!=std::string::npos);
  comandoHost("filtro 0"); assert(leerConteo().filtro==500); // PWM50: impedir cambio
  comandoHost("stop");
  const auto previoFiltro=leerConteo(); comandoHost("filtro 0");
  assert(leerConteo().filtro==0 && leerConteo().pulsos==0);
  assert(leerConteo().total==previoFiltro.total && leerConteo().crudos==previoFiltro.crudos);
  flanco(1001); flanco(1002); assert(leerConteo().total==previoFiltro.total+2);
  comandoHost("filtro 100000"); flanco(1003);
  assert(leerConteo().total==previoFiltro.total+2); // conserva ultimo aceptado
  comandoHost("filtro 500"); flanco(1499); flanco(1502);
  assert(leerConteo().total==previoFiltro.total+3);
  for(const char*s:{"filtro 100001","filtro -1","filtro 1.0","filtro abc","filtro 0 extra"}) {
    comandoHost(s); assert(leerConteo().filtro==500);
  }
  comandoHost("rampa"); assert(rampaActiva && pwmPct==0);
  comandoHost("filtro 0"); assert(leerConteo().filtro==500); // rampa en0 tambien bloquea
  relojMs+=3000; actualizarRampa(); assert(pwmPct==10 && rampaActiva);
  comandoHost("stop"); assert(!rampaActiva && pwmPct==0);
  relojMs+=3000; actualizarRampa(); assert(pwmPct==0);
  comandoHost("rampa"); comandoHost("vel 30"); assert(!rampaActiva && pwmPct==30);
  for (char c : std::string("vel 70")) Serial.entrada.push_back(c);
  servirSerial(); assert(pwmPct==30);
  Serial.entrada.push_back('\n'); servirSerial(); assert(pwmPct==70);
  for (char c : std::string("vel 90")+std::string(120,' ')+'\n') Serial.entrada.push_back(c);
  while (Serial.available()) servirSerial(); assert(pwmPct==70);
  // Concurrencia real host: 20k ISR frente a snapshots+resets, sin perder conteos.
  relojMs=0; reiniciarMedicion();
  portENTER_CRITICAL(&muxConteo); huboPulso=false; portEXIT_CRITICAL(&muxConteo);
  const uint32_t base=leerConteo().total;
  const uint32_t baseCrudos=leerConteo().crudos, baseMarca=cambiosMarca;
  std::atomic<bool> terminado{false};
  std::thread productor([&] {
    for (uint32_t i=1;i<=20000;++i) { relojUs=i*1000; relojMs=i; contarPulso(); }
    terminado=true;
  });
  uint32_t sumVentanas=0;
  while (!terminado) {
    EstadoConteo e;
    if (tomarVentana(e)) sumVentanas+=e.pulsos;
    const auto lectura=leerConteo();
    assert(lectura.total>=base && lectura.pulsos<=lectura.total-base);
  }
  productor.join();
  sumVentanas+=leerConteo().pulsos;
  assert(leerConteo().total-base==20000 && sumVentanas==20000);
  assert(leerConteo().crudos-baseCrudos==20000 && cambiosMarca-baseMarca==20000);
  // Rollover de contadores: rechazados se obtiene por resta modular consistente.
  portENTER_CRITICAL(&muxConteo);
  crudosTotal=UINT32_MAX; pulsosTotal=UINT32_MAX-1; huboPulso=false;
  portEXIT_CRITICAL(&muxConteo);
  flanco(0); assert(leerConteo().crudos==0 && leerConteo().total==UINT32_MAX && leerConteo().rechazados==1);
  flanco(500); assert(leerConteo().crudos==1 && leerConteo().total==0 && leerConteo().rechazados==1);
  writeOk=false; aplicarPWM(50); assert(!pwmListo && detached && nivelPin==HIGH && !rampaActiva);
  puts("OK: IRQ FALLING, crudos/aceptados/rechazados, filtroconfig0..100000, marcaGPIO5, snapshots20kconcurrentes, ADC, dt/rollovers, parser/RX/rampa/falloPWM.");
}

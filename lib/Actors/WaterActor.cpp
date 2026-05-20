/*
FILE: lib/Actors/WaterActor.cpp
Deberian saber que hay un orden para leer los comentarios, esto pq me canso.
Entonces ya no entrare en tanto detalle como en el HeatActor, asumiendo que ya entiendes mi Logica.
Sobre todo pq la bomba no necesita gestionar ciclos complejos de 12 horas como la lámpara.
Su trabajo es practicamente reactivo: "prende un ratico cuando haya sed, y apágate".
*/
#include "WaterActor.h"

//  Acta de Nacimiento
WaterActor::WaterActor(uint8_t pin)
    : _pin(pin), _active(false),
      _lastActivationMs(0), _activationStartMs(0), _totalOnTimeMs(0) {}
//Duración exacta de riego en milisegundos, y se lo SÚMA al acumulador global (_totalOnTimeMs).
void WaterActor::begin() {
    digitalWrite(_pin, RELAY_OFF);
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, RELAY_OFF);
    _active = false;
    Serial.printf("WaterActor: Relé bomba init GPIO%d\n", _pin);
}

//   Controles
bool WaterActor::activate() {
    if (_active) return false;
    digitalWrite(_pin, RELAY_ON);
    _active            = true;
    _lastActivationMs  = millis();
    _activationStartMs = millis();
    Serial.println(F("WaterActor: BOMBA → ON"));
    return true;
}
/*
Al apagar el pin eléctrico, el código calcula: "Tiempo actual (millis()) menos la hora en que prendió (_activationStartMs)". 
Eso te da la duración exacta de este riego en milisegundos, y se lo SÚMA al acumulador global (_totalOnTimeMs).
*/
void WaterActor::deactivate() {
    if (!_active) return;
    digitalWrite(_pin, RELAY_OFF);
    _totalOnTimeMs += (millis() - _activationStartMs);
    _active = false;
    Serial.println(F("WaterActor: BOMBA → OFF"));
}

//GETTERSSSzzzzzzzzz
bool          WaterActor::isActive()              const { return _active; }
unsigned long WaterActor::getLastActivationTime() const { return _lastActivationMs; }
/*
DATAZO: Si alguien pregunta el tiempo total de encendido mientras la bomba todavía está regando
el código hace matemática irt: toma el acumulador del pasado y le suma los milisegundos que lleva 
corriendo en este preciso instante. Si está apagada, solo devuelve el acumulador estático.
*/
unsigned long WaterActor::getTotalOnTimeMs()      const {
    return _totalOnTimeMs + (_active ? millis() - _activationStartMs : 0UL);
}

//   Depuracion
void WaterActor::debugForceActivate() {
    Serial.println(F("WaterActor: [DEBUG] Forzando BOMBA → ON (sin cooldown) - Peligro no?"));
    digitalWrite(_pin, RELAY_ON);
    _active            = true;
    _activationStartMs = millis();
}
void WaterActor::debugForceDeactivate() {
    Serial.println(F("WaterActor: [DEBUG] Forzando BOMBA → OFF"));
    digitalWrite(_pin, RELAY_OFF);
    _totalOnTimeMs += (_active ? millis() - _activationStartMs : 0UL);
    _active = false;
}
void WaterActor::printDebug() const {
    Serial.printf("WaterActor: GPIO%d | Estado=%-3s | TotalON=%lums\n",
                  _pin, _active ? "ON" : "OFF", getTotalOnTimeMs());
}
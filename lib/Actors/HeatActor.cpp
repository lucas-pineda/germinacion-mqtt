//FILE: lib/Actors/HeatActor.cpp
#include "HeatActor.h"

/*   Acta de Nacimiento
Cuando el sistema arranca, le asigna el pin físico (Ver Config.h)
dice que inicia apagado (_active(false)) y calcula cuántos milisegundos 
equivalen a 12 horas encendido y 12 horas apagado por defecto (12UL * 3600UL * 1000UL).
DATO: El uso de UL (Unsigned Long) obliga a C++ a calcular esa multiplicación matemática gigante usando 
memoria de 32 bits, evitando que el número se desborde antes de iniciar.
*/
HeatActor::HeatActor(uint8_t pin)
    : _pin(pin), _active(false),
      _lastActivationMs(0), _cycleStartMs(0),
      _onDurationMs(12UL * 3600UL * 1000UL),
      _offDurationMs(12UL * 3600UL * 1000UL),
      _cycleEnabled(false) {}

/*   Preparacion del Hardware
Configura eléctricamente la ESP32. 
Define el pin del foco como una salida de voltaje (OUTPUT) y 
se asegura de escribir un RELAY_OFF (que es HIGH según la lógica invertida) 
*/
void HeatActor::begin() {
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, RELAY_OFF);
    _active       = false;
    _cycleStartMs = millis();
    Serial.printf("Se inicio: HeatActor Relé foco init GPIO%d\n", _pin);
}

/*   Controles
Prenden y apagan el pin digital. 
Validación: Si el foco ya está encendido (if (_active)), el método se sale inmediatamente devolviendo false. 
Esto evita que la ESP32 gaste ciclos de reloj enviando la misma orden eléctrica una y otra vez en el bucle infinito.
*/
bool HeatActor::activate() {
    if (_active) return false;
    digitalWrite(_pin, RELAY_ON);
    _active           = true;
    _lastActivationMs = millis();
    Serial.println(F("HeatActor: FOCO → ON"));
    return true;
}
void HeatActor::deactivate() {
    if (!_active) return;
    digitalWrite(_pin, RELAY_OFF);
    _active = false;
    Serial.println(F("HeatActor: FOCO → OFF"));
}

//Getters (Todos sabemos q es esto no?)
bool          HeatActor::isActive()              const { return _active; }
unsigned long HeatActor::getLastActivationTime() const { return _lastActivationMs; }

/*   Ciclo Diario - Acta de Nacimiento
Este método actúa como un temporizador (un "Timer" asíncrono). 
*/
void HeatActor::setLightCycle(unsigned long onMs, unsigned long offMs) {
    _onDurationMs  = onMs;
    _offDurationMs = offMs;
    _cycleEnabled  = true;
    _cycleStartMs  = millis();
    Serial.printf("HeatActor; Ciclo de luz: ON=%luh  OFF=%luh\n",
                  onMs / 3600000UL, offMs / 3600000UL);
}
/*
Calcula cuántos milisegundos han pasado (elapsed) desde que inició el estado actual. 
Si el foco está encendido y el tiempo transcurrido supera las horas programadas (_onDurationMs)
invoca a deactivate(), resetea el cronómetro (_cycleStartMs = millis()) y avisa al sistema 
que hubo un cambio de estado retornando true.
*/
bool HeatActor::updateCycle() {
    if (!_cycleEnabled) return false;
    unsigned long elapsed = millis() - _cycleStartMs;
    if (_active && elapsed >= _onDurationMs) {
        deactivate();
        _cycleStartMs = millis();
        return true;
    }
    if (!_active && elapsed >= _offDurationMs) {
        activate();
        _cycleStartMs = millis();
        return true;
    }
    return false;
}

/*   Depuracion 
Métodos manuales que se activan directamente cuando se escriben comandos como LAMP ON en el modo debug. 
Ignoran cualquier otra regla del gestor de perfiles y fuerzan la señal eléctrica en el transistor/relé del hardware.
*/
void HeatActor::debugForceActivate() {
    Serial.println(F("HeatActor: [DEBUG] Forzando FOCO → ON (sin límites térmicos) - Peligroso no?"));
    digitalWrite(_pin, RELAY_ON);
    _active           = true;
    _lastActivationMs = millis();
}
void HeatActor::debugForceDeactivate() {
    Serial.println(F("HeatActor: [DEBUG] Forzando FOCO → OFF"));
    digitalWrite(_pin, RELAY_OFF);
    _active = false;
}
void HeatActor::printDebug() const {
    Serial.printf("HeatActor:  GPIO%d | Estado=%-3s | CicloActivo=%s\n",
                  _pin, _active ? "ON" : "OFF", _cycleEnabled ? "si" : "no");
}
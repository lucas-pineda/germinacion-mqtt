/*
FILE: lib/Actors/HeatActor.h
Controla el relé del foco/calefactor en GPIO18.
Para mejor entendimiento de esta clase referirse a HeatActor.cpp
*/
#ifndef HEAT_ACTOR_H
#define HEAT_ACTOR_H

#include "../../include/Config.h"
#include <Arduino.h>

class HeatActor {
public:
    explicit HeatActor(uint8_t pin = PIN_RELAY_LAMP);
    void begin();
    bool activate();
    void deactivate();
    bool          isActive()              const;
    unsigned long getLastActivationTime() const;

    // Configura el ciclo de luz (tiempos en ms)
    void setLightCycle(unsigned long onMs, unsigned long offMs);
    // Llama en cada iteración del loop. Retorna true si cambió de estado.
    bool updateCycle();

    void debugForceActivate();
    void debugForceDeactivate();
    void printDebug() const;
private:
    uint8_t       _pin;
    bool          _active;
    unsigned long _lastActivationMs;
    unsigned long _cycleStartMs;
    unsigned long _onDurationMs;
    unsigned long _offDurationMs;
    bool          _cycleEnabled;
};

#endif // HEAT_ACTOR_H
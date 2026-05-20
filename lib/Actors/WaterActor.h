/*
FILE: lib/Actors/WaterActor.h
Controla el relé de la bomba de agua en GPIO5.
Para mejor entendimiento de esta clase referirse a WaterActor.cpp
*/
#ifndef WATER_ACTOR_H
#define WATER_ACTOR_H

#include "../../include/Config.h"
#include <Arduino.h>

class WaterActor {
public:
    explicit WaterActor(uint8_t pin = PIN_RELAY_PUMP);
    void begin();
    bool activate();
    void deactivate();
    bool          isActive()             const;
    unsigned long getLastActivationTime()const;
    unsigned long getTotalOnTimeMs()     const;

    // Debug: activa/desactiva ignorando cualquier lógica externa
    void debugForceActivate();
    void debugForceDeactivate();
    void printDebug() const;
private:
    uint8_t       _pin;
    bool          _active;
    unsigned long _lastActivationMs;
    unsigned long _activationStartMs;
    unsigned long _totalOnTimeMs;
};

#endif // WATER_ACTOR_H
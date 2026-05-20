// FILE: lib/Sensors/Sensor.h
#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>
enum class SensorStatus : uint8_t {
    OK           = 0,
    NOT_READY    = 1,
    READ_ERROR   = 2,
    OUT_OF_RANGE = 3,
    SIMULATED    = 4   
};

/*  Acta de Nacimiento - Sensor Base
Para que el programa principal no tenga que saber si está leyendo un sensor de agua, de temperatura o de luz. 
Al usar una clase base, defini un "contrato" (begin(), read()). 
Si mañana se cambia el hardware, no se tiene que tocar el resto del código, solo añadir la subclase del sensor nuevo.
*/
class Sensor {
public:
    explicit Sensor(uint8_t pin) : _pin(pin), _status(SensorStatus::NOT_READY) {}
    virtual ~Sensor() = default;

    virtual bool begin() = 0;
    virtual bool read()  = 0;

    bool isValid() const {
        return _status == SensorStatus::OK || _status == SensorStatus::SIMULATED;
    }

    SensorStatus getStatus()  const { return _status; }
    uint8_t      getPin()     const { return _pin; }

    const char* statusString() const {
        switch (_status) {
            case SensorStatus::OK:           return "OK";
            case SensorStatus::NOT_READY:    return "NOT_READY";
            case SensorStatus::READ_ERROR:   return "READ_ERROR";
            case SensorStatus::OUT_OF_RANGE: return "OUT_OF_RANGE";
            case SensorStatus::SIMULATED:    return "SIMULATED";
            default:                         return "UNKNOWN";
        }
    }

protected:
    uint8_t      _pin;
    SensorStatus _status;
};

#endif // SENSOR_H
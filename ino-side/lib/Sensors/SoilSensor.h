// FILE: lib/Sensors/SoilSensor.h
#ifndef SOIL_SENSOR_H
#define SOIL_SENSOR_H

// convierte la lectura ADC (12 bits) a porcentaje de humedad (0–100%).
#include "../../include/Config.h"
#include "Sensor.h"

class SoilSensor : public Sensor {
public:
    explicit SoilSensor(uint8_t pin    = PIN_SOIL_MOISTURE,
                        int     adcDry = SOIL_ADC_DRY,
                        int     adcWet = SOIL_ADC_WET);

    bool begin() override;
    bool read()  override;

    // retorna humedad de suelo en % (0.0–100.0). -1 si no es válido.
    float getMoisturePercent() const;
    int   getRawADC()          const;
    // mock para modo debug
    void setSimulatedValue(float percent);
    void clearSimulation();
    bool isSimulated() const { return _simulated; }

    void printDebug() const;
private:
    float _moisturePercent;
    int   _rawADC;
    int   _adcDry;
    int   _adcWet;
    bool  _simulated;
    float _simulatedValue;

    float _mapToPercent(int raw) const;
};

#endif // SOIL_SENSOR_H
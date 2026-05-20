//FILE: lib/Sensors/TempSensor.h
#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#include "../../include/Config.h"
#include "Sensor.h"
#include <DHT.h>

class TempSensor : public Sensor {
public:
    explicit TempSensor(uint8_t pin     = PIN_DHT22,
                        uint8_t dhtType = DHT_TYPE); //Define el PIN
    bool begin() override;//Empieza las lecturas
    bool read()  override;//Las lee
    float getTemperature() const;//Dado que el sensor detecta temperatura y humedad, se separaron los datos, entonces
    float getHumidity()    const;//Se tiene x un lado la humedad y x otro la temperatura
    void setSimulatedValues(float tempC, float humidity);//Valores falsos para pruebas
    void clearSimulation();//Limpieza del test
    bool isSimulated() const { return _simulated; }

    void printDebug() const;//Depuracion
private:
    DHT   _dht;
    float _temperature;
    float _humidity;
    bool  _simulated;
    float _simTemp;
    float _simHumidity;
};

#endif // TEMP_SENSOR_H
#include "TempSensor.h"

TempSensor::TempSensor(uint8_t pin, uint8_t dhtType)
    : Sensor(pin),
      _dht(pin, dhtType),
      _temperature(NAN),
      _humidity(NAN),
      _simulated(false),
      //Esto es a lo q me refiero con valores falsos
      //(En caso de q estes leyendo esto)
      _simTemp(25.0f),
      _simHumidity(50.0f) {}

bool TempSensor::begin() {
    _dht.begin();
    _status = SensorStatus::NOT_READY;
    Serial.printf("TempSensor: DHT22 init GPIO%d\n", _pin);
    return true;
}
bool TempSensor::read() {
    if (_simulated) {
        _temperature = _simTemp;
        _humidity    = _simHumidity;
        _status      = SensorStatus::SIMULATED;
        return true;
    }
    float t = _dht.readTemperature();
    float h = _dht.readHumidity();

    //Esto es x si falla y no detecta nada
    if (isnan(t) || isnan(h)) {
        _status      = SensorStatus::READ_ERROR;
        _temperature = NAN;
        _humidity    = NAN;
        Serial.println(F("TempSensor: ERROR – DHT22 no responde."));
        return false;
    }

    if (t < -10.0f || t > 80.0f || h < 0.0f || h > 100.0f) {
        _status = SensorStatus::OUT_OF_RANGE;
        Serial.printf("TempSensor: FUERA_DE_RANGO – T=%.1f  H=%.1f\n", t, h);
        return false;
    }

    _temperature = t;
    _humidity    = h;
    _status      = SensorStatus::OK;
    return true;
}

//Getters
float TempSensor::getTemperature() const { return isValid() ? _temperature : NAN; }
float TempSensor::getHumidity()    const { return isValid() ? _humidity    : NAN; }

//Valores fake para debug
void TempSensor::setSimulatedValues(float tempC, float humidity) {
    _simTemp     = tempC;
    _simHumidity = constrain(humidity, 0.0f, 100.0f);
    _simulated   = true;
    _temperature = _simTemp;
    _humidity    = _simHumidity;
    _status      = SensorStatus::SIMULATED;
    Serial.printf("TempSensor: [MOCK] Temp=%.1f°C  Hum=%.1f%%\n", _simTemp, _simHumidity);
}
void TempSensor::clearSimulation() {
    _simulated = false;
    _status    = SensorStatus::NOT_READY;
    Serial.println(F("TempSensor: Simulación desactivada. Usando hardware real."));
}

//Depuracion
void TempSensor::printDebug() const {
    Serial.printf("TempSensor:  Status=%-10s | Temp=%6.2f°C | Hum=%5.1f%%\n",
                  statusString(), _temperature, _humidity);
}
// FILE: lib/Sensors/SoilSensor.cpp
#include "SoilSensor.h"

SoilSensor::SoilSensor(uint8_t pin, int adcDry, int adcWet)
    : Sensor(pin),
      _moisturePercent(-1.0f),
      _rawADC(0),
      _adcDry(adcDry),
      _adcWet(adcWet),
      _simulated(false),
      _simulatedValue(0.0f) {}
bool SoilSensor::begin() {
    pinMode(_pin, INPUT);
    _status = SensorStatus::NOT_READY;
    Serial.printf("Soil: sensor capacitivo init GPIO%d | seco=%d humedo=%d\n",
                  _pin, _adcDry, _adcWet);
    return true;
}

//Lectura incial
bool SoilSensor::read() {
    if (_simulated) {
        _moisturePercent = _simulatedValue;
        _status          = SensorStatus::SIMULATED;
        return true;
    }

    long sum = 0;
    for (int i = 0; i < SOIL_OVERSAMPLE; i++) {
        sum += analogRead(_pin);
        delay(2);
    }
    _rawADC = static_cast<int>(sum / SOIL_OVERSAMPLE);

    // un valor en los extremos absolutos del ADC indica fallo de cableado
    if (_rawADC <= 0 || _rawADC >= 4095) {
        _status          = SensorStatus::READ_ERROR;
        _moisturePercent = -1.0f;
        Serial.printf("Soil: ERROR — ADC fuera de rango: %d (verifica GPIO34 o prueba GPIO14)\n",
                      _rawADC);
        return false;
    }

    _moisturePercent = _mapToPercent(_rawADC);
    _status          = SensorStatus::OK;
    return true;
}

//Getters
float SoilSensor::getMoisturePercent() const {
    return isValid() ? _moisturePercent : -1.0f;
}
int SoilSensor::getRawADC() const { return _rawADC; }

//Calculo
float SoilSensor::_mapToPercent(int raw) const {
    // sensor capacitivo: tensión alta = seco, tensión baja = húmedo → inversión
    float pct = static_cast<float>(_adcDry - raw) /
                static_cast<float>(_adcDry - _adcWet) * 100.0f;
    return constrain(pct, 0.0f, 100.0f);
}
//Empieza una simulacion
void SoilSensor::setSimulatedValue(float percent) {
    _simulatedValue  = constrain(percent, 0.0f, 100.0f);
    _simulated       = true;
    _moisturePercent = _simulatedValue;
    _status          = SensorStatus::SIMULATED;
    Serial.printf("Soil: mock → humedad suelo forzada a %.1f%%\n", _simulatedValue);
}

//Limpia la Simulacion
void SoilSensor::clearSimulation() {
    _simulated = false;
    _status    = SensorStatus::NOT_READY;
    Serial.println("Soil: simulación desactivada");
}

//Debug compiracion lo q sea
void SoilSensor::printDebug() const {
    Serial.printf("Soil: status=%-10s | ADC=%4d | humedad=%.1f%%\n",
                  statusString(), _rawADC, _moisturePercent);
}
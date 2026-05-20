// FILE: lib/ProfileManager/ProfileManager.cpp
#include "ProfileManager.h"
#include "SoilSensor.h"
#include "TempSensor.h"
#include "WaterActor.h"

/*   Matriz de Sensibilidad
Cruza el requerimiento de agua (filas) con el de sol (columnas) 
para calcular un Nivel de Sensibilidad del 1 al 9
*/
const uint8_t ProfileManager::SENSIBILITY_MATRIX[3][3] = {
    {1, 2, 3},
    {4, 5, 6},
    {7, 8, 9}
};

/*  Acta de Nacimiento - Manager de Perfiles
Cuando se usa setProfile(water, sun), C++ convierte los enums (LOW, MID, HIGH) a números (0, 1, 2) 
usando static_cast<uint8_t>. Por ejemplo, si configuras tu perfil como Agua MEDIA (1) y Sol ALTO (2)
el sistema busca en la matriz la posición [1][2] y te asigna automáticamente un Nivel de Sensibilidad 6.
*/
ProfileManager::ProfileManager()
    : _waterLevel(WaterLevel::WATER_MID),
      _sunLevel(SunLevel::SUN_MID),
      _sensibilityLevel(5) {}
void ProfileManager::setProfile(WaterLevel water, SunLevel sun) {
    _waterLevel       = water;
    _sunLevel         = sun;
    _sensibilityLevel = SENSIBILITY_MATRIX[static_cast<uint8_t>(water)]
                                          [static_cast<uint8_t>(sun)];
    Serial.printf("Profile: agua=%-5s  sol=%-5s  nivel=%d\n",
                  waterLevelName(water), sunLevelName(sun), _sensibilityLevel);
}
uint8_t ProfileManager::getSensibilityLevel() const { return _sensibilityLevel; }
unsigned long ProfileManager::getSampleIntervalMs() const {
    unsigned long step = (SAMPLE_INTERVAL_MAX_MS - SAMPLE_INTERVAL_MIN_MS) / 8UL;
    return SAMPLE_INTERVAL_MAX_MS - ((_sensibilityLevel - 1UL) * step);
}
const char* ProfileManager::waterLevelName(WaterLevel l) {
    switch (l) {
        case WaterLevel::WATER_LOW:  return "BAJA";
        case WaterLevel::WATER_MID:  return "MEDIA";
        case WaterLevel::WATER_HIGH: return "ALTA";
        default: return "?";
    }
}
const char* ProfileManager::sunLevelName(SunLevel l) {
    switch (l) {
        case SunLevel::SUN_LOW:  return "BAJO";
        case SunLevel::SUN_MID:  return "MEDIO";
        case SunLevel::SUN_HIGH: return "ALTO";
        default: return "?";
    }
}

//Depuracion
void ProfileManager::printDebug() const {
    Serial.printf("Profile: agua=%-5s  sol=%-5s  nivel=%d  intervalo=%lus\n",
                  waterLevelName(_waterLevel), sunLevelName(_sunLevel),
                  _sensibilityLevel, getSampleIntervalMs() / 1000UL);
}

/*   Acta de Nacimiento
Entramos al constructor de hidratación. Cuando se inicializa, ejecuta la función privada _applyProfile(level).
Esta función carga en memoria un struct con los límites físicos exactos del riego según el nivel elegido:
- WATER_LOW (Plantas de clima seco. Ej: Cactus): Se activa si la humedad baja de 2.0 y se apaga al llegar a 4.0 (Histéresis estrecha).
 * Tiene un Cooldown masivo de 24 horas (24UL * 3600000UL). La bomba no volverá a prender en un día entero para evitar ahogar la raíz.
 * Detecta anomalías si la humedad cae un 2.0 en menos de 6 horas.
- WATER_MID (Configuración estándar / Brotes comunes):
 * Rango de operación entre 4.0 y 7.0.
 * Descanso obligatorio de 12 horas entre riegos.
 * Alerta de fuga si cae 3.0 unidades en menos de 4 horas.
- WATER_HIGH (Plantas de alta humedad / Semillas acuáticas):
 * Rango alto entre 7.0 y 9.0.
 * Solo espera 6 horas de descanso antes de poder volver a regar.
 * Es súper estricto: si la humedad cae un 2.0 en menos de 2 horas, 
 * asume de inmediato que el agua se está evaporando o saliendo del contenedor y bloquea el sistema.
*/
HydrationController::HydrationController(SoilSensor& soil,
                                         WaterActor& actor,
                                         WaterLevel  level)
    : _soil(soil), _actor(actor),
      _lastDeactivationMs(0),
      _gradientWindowStartMs(0),
      _soilAtWindowStart(-1.0f)
{
    _applyProfile(level);
}

/*  Cooldown (Calculo de Sueño)
Calcula dinámicamente cada cuánto tiempo debe la ESP32 despertar para revisar los sensores. 
No usa valores fijos, sino que hace una interpolación lineal basada en el nivel de sensibilidad (1 al 9).
Ejemplo de Funcionamiento:
- Divide el total (30 min - 5 min = 25 minutos) entre los 8 escalones que hay entre el nivel 1 y el 9. 
Cada paso (step) equivale a unos 3.12 minutos.
- Si estás en el Nivel 1 (poca prioridad): La resta da 0 * step, devolviendo el tiempo máximo (30 minutos de sueño).
- Si estás en el Nivel 9 (máxima prioridad): Resta 8 * step (los 25 minutos completos), bajando el tiempo al mínimo (5 minutos de sueño).
*/
void HydrationController::_applyProfile(WaterLevel level) {
    // umbrales en % de humedad de suelo
    // activar bomba cuando el suelo baja de soilActivateAt
    // apagar cuando sube a soilDeactivateAt
    switch (level) {
        case WaterLevel::WATER_LOW:
            // plantas de bajo riego: regar a 20%, parar a 35%
            _profile = { 20.0f, 35.0f, 24UL * 3600000UL, 15.0f, 6UL * 3600000UL };
            break;
        case WaterLevel::WATER_MID:
            // plantas de riego medio: regar a 35%, parar a 55%
            _profile = { 35.0f, 55.0f, 12UL * 3600000UL, 15.0f, 4UL * 3600000UL };
            break;
        case WaterLevel::WATER_HIGH:
            // plantas de alto riego: regar a 55%, parar a 75%
            _profile = { 55.0f, 75.0f,  6UL * 3600000UL, 15.0f, 2UL * 3600000UL };
            break;
    }
}
bool HydrationController::isCooldownActive() const {
    if (_lastDeactivationMs == 0) return false;
    return (millis() - _lastDeactivationMs) < _profile.cooldownMs;
}

/*
Si el sensor se desconecta, se daña o empieza a mandar lecturas corruptas (nan)
el controlador se apaga inmediatamente por hardware (_actor.deactivate()) y frena el bucle. 
Evita que el rele se quede encendido indefinidamente al no tener lecturas de control.
*/
bool HydrationController::checkSafety() {
    if (!_soil.isValid()) {
        Serial.println("Hydration: sensor de suelo no válido — bomba desactivada");
        _actor.deactivate();
        return false;
    }
    return true;
}
bool HydrationController::update() {
    if (!_soil.read()) {
        Serial.println("Hydration: error leyendo sensor de suelo");
        return false;
    }
    if (!checkSafety()) return false;

    float soilPct = _soil.getMoisturePercent();
    _checkAnomaly(soilPct);

    bool toggled = false;

    if (!_actor.isActive()) {
        if (soilPct < _profile.soilActivateAt) {
            if (isCooldownActive()) {
                unsigned long rem = (_profile.cooldownMs -
                    (millis() - _lastDeactivationMs)) / 1000UL;
                Serial.printf("[hydration] cooldown activo — faltan %lus\n", rem);
            } else {
                _actor.activate();
                toggled = true;
            }
        }
    } else {
        if (soilPct >= _profile.soilDeactivateAt) {
            _actor.deactivate();
            _lastDeactivationMs = millis();
            toggled = true;
        }
    }
    return toggled;
}
void HydrationController::_checkAnomaly(float soilPct) {
    unsigned long now = millis();
    if (_gradientWindowStartMs == 0) {
        _gradientWindowStartMs = now;
        _soilAtWindowStart     = soilPct;
        return;
    }
    if ((now - _gradientWindowStartMs) >= _profile.anomalyWindowMs) {
        float drop = _soilAtWindowStart - soilPct;
        if (drop >= _profile.anomalyDrop && !_anomalyFlag) {
            Serial.printf("Hydration: Anomalía — suelo bajó %.1f%% en la ventana de tiempo\n",
                          drop);
            _anomalyFlag = true;
        }
        _gradientWindowStartMs = now;
        _soilAtWindowStart     = soilPct;
    }
}
void HydrationController::handleSerialCommand(const String& cmd) {
    if (!_testMode) {
        Serial.println("Hydration: Requiere TEST ON");
        return;
    }
    if (cmd.equalsIgnoreCase("PUMP ON"))  { _actor.debugForceActivate();   return; }
    Serial.printf("Hydration: bomba ahora: %s (GPIO%d)\n",
                  _actor.isActive() ? "ON" : "OFF", PIN_RELAY_PUMP);
    if (cmd.equalsIgnoreCase("PUMP OFF")) {
        _actor.debugForceDeactivate();
        _lastDeactivationMs = 0;
        Serial.printf("Hydration: bomba ahora: %s (GPIO%d)\n",
                  _actor.isActive() ? "ON" : "OFF", PIN_RELAY_PUMP);
        return;
    }
    Serial.println("Hydration: [Comandos] PUMP ON & PUMP OFF");
}
void HydrationController::printDebug() const {
    Serial.printf("Hydration: cooldown=%-3s  anomalía=%-3s  bomba=%-3s\n",
                  isCooldownActive() ? "SI" : "NO",
                  _anomalyFlag       ? "SI" : "NO",
                  _actor.isActive()  ? "ON" : "OFF");
}

// ThermalAdvisor — monitorea temperatura del aire y sugiere foco (Antes esto era mucho mas complejo pero bueno)
ThermalAdvisor::ThermalAdvisor(TempSensor& sensor, SunLevel level)
    : _sensor(sensor),
      _anomalyFlag(false),
      _focoSugerido(false),
      _tempActual(NAN),
      _humActual(NAN),
      _alertStartMs(0)
{
    _applyProfile(level);
}
void ThermalAdvisor::_applyProfile(SunLevel level) {
    switch (level) {
        case SunLevel::SUN_LOW:
            _profile = { 18.0f, 22.0f, 1.0f, 25.0f, 1UL * 3600000UL, 40.0f, 65.0f };
            break;
        case SunLevel::SUN_MID:
            _profile = { 22.0f, 26.0f, 1.0f, 30.0f, 30UL * 60000UL,  50.0f, 70.0f };
            break;
        case SunLevel::SUN_HIGH:
            _profile = { 26.0f, 32.0f, 2.0f, 35.0f, 15UL * 60000UL,  55.0f, 75.0f };
            break;
    }
}
void ThermalAdvisor::update() {
    if (!_sensor.isValid()) return;

    _tempActual = _sensor.getTemperature();
    _humActual  = _sensor.getHumidity();
    if (isnan(_tempActual)) return;

    // sugerencia de foco por histéresis
    if (_tempActual < _profile.tempMin) {
        _focoSugerido = true;
    } else if (_tempActual > (_profile.tempMax + _profile.hysteresisBuffer)) {
        _focoSugerido = false;
    }

    _checkAlert(_tempActual);
}
void ThermalAdvisor::_checkAlert(float temp) {
    if (temp > _profile.alertTempC) {
        if (_alertStartMs == 0) _alertStartMs = millis();
        if ((millis() - _alertStartMs) >= _profile.alertWindowMs && !_anomalyFlag) {
            Serial.printf("[ALERTA] Temperatura alta: %.1f°C — hace falta ventilacion!\n", temp);
            _anomalyFlag = true;
        }
    } else {
        _alertStartMs = 0;
        if (_anomalyFlag) {
            _anomalyFlag = false;
            Serial.println("Thermal: temperatura normalizada");
        }
    }
}

//Debugracion lol quiero dormir
bool        ThermalAdvisor::focoSugerido()      const { return _focoSugerido; }
const char* ThermalAdvisor::focoSugeridoTexto() const { return _focoSugerido ? "ON" : "OFF"; }
void ThermalAdvisor::printDebug() const {
    Serial.printf("Thermal: temp=%.1f°C  rango=[%.0f-%.0f°C]  foco sugerido=%-3s\n",
                  _tempActual, _profile.tempMin, _profile.tempMax, focoSugeridoTexto());
}
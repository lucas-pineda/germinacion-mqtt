// FILE: lib/ProfileManager/ProfileManager.h
#ifndef PROFILE_MANAGER_H
#define PROFILE_MANAGER_H

#include <Arduino.h>
class SoilSensor;
class TempSensor;
class WaterActor;

enum class WaterLevel : uint8_t { WATER_LOW = 0, WATER_MID = 1, WATER_HIGH = 2 };
enum class SunLevel   : uint8_t { SUN_LOW   = 0, SUN_MID   = 1, SUN_HIGH   = 2 };

/*
Structs de Perfiles
Son contenedores de datos puros. No hacen cálculos, solo guardan las "reglas" que cambian según la planta.
(un cactus no tiene las mismas reglas que un brote de tomate).
*/
struct WaterProfile {
    float         soilActivateAt;       // % suelo — activa bomba por debajo de este valor
    float         soilDeactivateAt;     // % suelo — apaga bomba al superar este valor
    unsigned long cooldownMs;
    float         anomalyDrop;          // caída de % en la ventana que dispara anomalía
    unsigned long anomalyWindowMs;
};
struct SunProfile {
    float         tempMin;
    float         tempMax;
    float         hysteresisBuffer;
    float         alertTempC;           // temperatura que dispara alerta de ventilación
    unsigned long alertWindowMs;
    float         humMin;               // % humedad aire mínima esperada (solo monitoreo)
    float         humMax;               // % humedad aire máxima esperada (solo monitoreo)
};

/*   Clase Base: CONTROLLER
Clase abstracta. Funciona como un contrato o una plantilla ley para los controladores específicos. 
Al tener funciones igualadas a cero (= 0), obliga a que cualquier controlador derivado implemente obligatoriamente la lógica de:
-update(): Ejecutar la máquina de estados.
-checkSafety(): Verificar que la planta no esté en peligro.
-isCooldownActive(): Saber si el actuador está descansando.
Además, centraliza las banderas del Modo Debug (_testMode) y de fallos globales (_anomalyFlag) 
para que ambos controladores hablen el mismo idioma.
*/
class Controller {
public:
    Controller() : _anomalyFlag(false), _testMode(false) {}
    virtual ~Controller() = default;

    virtual bool update()           = 0;
    virtual bool checkSafety()      = 0;
    virtual bool isCooldownActive() const = 0;

    bool hasAnomaly()  const { return _anomalyFlag; }
    void clearAnomaly()      { _anomalyFlag = false; }
    void setTestMode(bool v) { _testMode = v; }
    bool isTestMode()  const { return _testMode; }

protected:
    bool _anomalyFlag;
    bool _testMode;
};

/*  Subclases del Controller
Son los verdaderos operarios. Reciben por referencia los sensores y actores del sistema 
para poder manipularlos directamente sin duplicar memoria RAM.
- HydrationController: Une el SoilSensor con el WaterActor para decidir cuándo 
abrir la llave del agua y monitorear la velocidad de absorción.
- ThermalAdvisor: evalúa temperatura del aire y genera sugerencia de foco + alerta de ventilación
*/
class HydrationController : public Controller {
public:
    HydrationController(SoilSensor& soil, WaterActor& actor, WaterLevel level);

    bool update()           override;
    bool checkSafety()      override;
    bool isCooldownActive() const override;

    void handleSerialCommand(const String& cmd);
    void printDebug() const;

    float getSoilActivateAt()   const { return _profile.soilActivateAt; }
    float getSoilDeactivateAt() const { return _profile.soilDeactivateAt; }

private:
    SoilSensor&  _soil;
    WaterActor&  _actor;
    WaterProfile _profile;

    unsigned long _lastDeactivationMs;
    unsigned long _gradientWindowStartMs;
    float         _soilAtWindowStart;

    void _applyProfile(WaterLevel level);
    void _checkAnomaly(float soilPct);
};
class ThermalAdvisor {
public:
    ThermalAdvisor(TempSensor& sensor, SunLevel level);

    void update();

    bool        focoSugerido()      const;
    const char* focoSugeridoTexto() const;

    bool hasAnomaly()   const { return _anomalyFlag; }
    void clearAnomaly()       { _anomalyFlag = false; }

    float getTempActual() const { return _tempActual; }
    float getHumActual()  const { return _humActual;  }
    float getTempMin()    const { return _profile.tempMin; }
    float getTempMax()    const { return _profile.tempMax; }
    float getHumMin()     const { return _profile.humMin; }
    float getHumMax()     const { return _profile.humMax; }

    void printDebug() const;

private:
    TempSensor&  _sensor;
    SunProfile   _profile;
    bool         _anomalyFlag;
    bool         _focoSugerido;
    float        _tempActual;
    float        _humActual;
    unsigned long _alertStartMs;

    void _applyProfile(SunLevel level);
    void _checkAlert(float temp);
};

/* Clase Maestra
Este es el "Selector de Modos". No lee sensores ni prende relés; su única misión es cruzar 
los niveles de agua y sol elegidos para calcular la prioridad y el estrés del sistema.
- SENSIBILITY_MATRIX[3][3]: Es una matriz estática de 3x3 que cruza los estados (BAJO, MEDIO, ALTO) de Agua y Sol. 
Al cruzarlos, genera un número del 1 al 9 (Nivel de Sensibilidad).
Usa ese nivel del 1 al 9 para calcular dinámicamente el tiempo de muestreo que se configuro antes.
Si la sensibilidad es alta (9), el intervalo de sueño se reduce para vigilar de cerca; 
Si es baja (1), la ESP32 espacia las lecturas para ahorrar energía.
*/
class ProfileManager {
public:
    ProfileManager();

    void    setProfile(WaterLevel water, SunLevel sun);
    uint8_t getSensibilityLevel()   const;
    unsigned long getSampleIntervalMs() const;

    WaterLevel getWaterLevel() const { return _waterLevel; }
    SunLevel   getSunLevel()   const { return _sunLevel; }

    static const char* waterLevelName(WaterLevel l);
    static const char* sunLevelName(SunLevel l);

    void printDebug() const;

private:
    WaterLevel _waterLevel;
    SunLevel   _sunLevel;
    uint8_t    _sensibilityLevel;

    static const uint8_t SENSIBILITY_MATRIX[3][3];
};

#endif // PROFILE_MANAGER_H
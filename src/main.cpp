#include <Arduino.h>
#include "Config.h"
#include "SoilSensor.h"
#include "TempSensor.h"
#include "WaterActor.h"
#include "HeatActor.h"
#include "ProfileManager.h"
#include "NetworkManager.h"

// AutoGerminator v3.1
// DHT22 en GPIO4 (temp+hum aire) | Suelo capacitivo en GPIO34 | Bomba en GPIO5
/*    Comandos disponibles (para Debug)
    STATUS            — estado completo con advertencias
    STATUS HIGH       — bucle de datos crudos de sensores cada 2s (sale con cualquier comando)
    REVIEW TEMP       — bucle solo temperatura del aire cada 2.5s
    REVIEW HUM        — bucle solo humedad del aire cada 2.5s
    REVIEW SOIL       — bucle solo humedad del suelo cada 2.5s
    PROFILE           — muestra perfil activo con umbrales
    PROFILE H M       — cambia perfil (solo en TEST ON) ej: agua=H sol=M
    ANOMALY CLEAR     — resetea flags de anomalía (caída brusca de suelo o alerta térmica)
    TEST ON           — activa modo debug, pausa ciclo automático
    TEST OFF          — desactiva modo debug
    PUMP ON / PUMP OFF — control manual de bomba (solo en TEST ON)
*/

enum class ReviewMode : uint8_t {
    NONE,
    TEMP,
    HUM,
    SOIL,
    ALL
};

/*  Acta de Nacimiento - CoreEngine
Inicializa por lista de asignación los sensores y actuadores estáticos
Pone en 0 los cronómetros de milisegundos y arranca el sistema con los modos de debug y review apagados.
El destructor libera la memoria dinámica borrando las instancias de _hydrationCtrl y _thermalAdvisor
para evitar fugas de memoria en la ESP32.
*/
class CoreEngine {
public:
    CoreEngine()
        : _soilSensor(),
          _tempSensor(),
          _waterActor(),
          _heatActor(),
          _profileManager(),
          _network(nullptr),
          _hydrationCtrl(nullptr),
          _thermalAdvisor(nullptr),
          _lastSampleMs(0),
          _lastReviewMs(0),
          _debugMode(false),
          _reviewMode(ReviewMode::NONE)
    {}
    ~CoreEngine() {
        delete _hydrationCtrl;
        delete _thermalAdvisor;
        delete _network;
    }

    /*
    Abre el puerto serial a la velocidad de la configuración previa (Eso ya se explico como valores simulados o fakes)
    y escupe un mensaje inicial mostrando qué pines físicos se asignaron.
    Inicializa los controladores de hardware (sensores y relés), monta por defecto el perfil medio de agua/sol,
    e imprime el menú de ayuda en la terminal al arrancar.
    */
    void begin() {
        Serial.begin(SERIAL_BAUD_RATE);
        delay(500);
        Serial.println();
        Serial.println("AutoGerminator" FW_VERSION);
        Serial.printf(
            "Config: DHT22=GPIO%d | Suelo=GPIO%d | Bomba=GPIO%d\n",
            PIN_DHT22,
            PIN_SOIL_MOISTURE,
            PIN_RELAY_PUMP
        );
        _soilSensor.begin();
        _tempSensor.begin();
        _waterActor.begin();
        _heatActor.begin();
        _applyProfile(
            WaterLevel::WATER_MID,
            SunLevel::SUN_MID
        );
        _network = new NetworkManager(
            WIFI_SSID, WIFI_PASSWORD, MQTT_BROKER_IP, MQTT_BROKER_PORT,
            _soilSensor, _tempSensor, _waterActor, _heatActor, _profileManager
        );
        _network->begin();
        _network->setCommandCallback([this](const String& cmd) {
            this->handleCommand(cmd);
        });
        _printHelp();
    }

    /*  Flujo de Programa en 3 capas
    - Escucha Serial: Captura comandos entrantes por teclado de inmediato.
    - Modo Review Activo (TEST ON): Si el usuario pide telemetría, se adueña del loop y lee/imprime
    los sensores de forma arcaica en ráfagas de 2 segundos (STATUS HIGH) o 2.5 segundos (individuales),
    cortando el resto del automatismo.
    - Modo Automático Normal: Si no hay review ni debug, evalúa mediante millis() si ya se cumplió el tiempo
    de muestreo del perfil para correr el ciclo de control inteligente (_runCycle()).
    */
    void loop() {
        if (_network)
            _network->update();
        if (Serial.available()) {
            String cmd = Serial.readStringUntil('\n');
            cmd.trim();
            if (cmd.length() > 0)
                _handleCommand(cmd);
        }

        // bucle de review — cualquier modo distinto de NONE
        if (_reviewMode != ReviewMode::NONE) {
            unsigned long interval =
                (_reviewMode == ReviewMode::ALL)
                ? 2000UL
                : 2500UL;

            if (millis() - _lastReviewMs >= interval) {
                _lastReviewMs = millis();
                _soilSensor.read();
                _tempSensor.read();
                _printReview();
            }
            return;
        }
        if (_debugMode)
            return;
        unsigned long now = millis();
        if ((now - _lastSampleMs) >= _profileManager.getSampleIntervalMs()) {
        _lastSampleMs = now;
        Serial.printf("[Loop] now=%lu lastSample=%lu intervalo=%lu\n",
                    now, _lastSampleMs, _profileManager.getSampleIntervalMs());
        _runCycle();
        }
    }

    // puente público para que NetworkManager inyecte comandos MQTT
    void handleCommand(const String& cmd) {
        _handleCommand(cmd);
    }
private:
    SoilSensor     _soilSensor;
    TempSensor     _tempSensor;
    WaterActor     _waterActor;
    HeatActor      _heatActor;
    ProfileManager _profileManager;
    NetworkManager* _network;
    HydrationController* _hydrationCtrl;
    ThermalAdvisor*      _thermalAdvisor;
    unsigned long _lastSampleMs;
    unsigned long _lastReviewMs;
    bool          _debugMode;
    ReviewMode    _reviewMode;

    /*
    Construye controladores en base al perfil (en base al nivel de sensibilidad).
    Libera la memoria RAM ocupada por los controladores anteriores usando delete y vuelve a instanciarlos con new,
    inyectándoles el sensor de suelo real, los actuadores correspondientes y las nuevas restricciones de la matriz de sensibilidad.
    Finalmente, propaga el estado actual del modo debug al controlador de hidratación.
    */
    void _applyProfile(WaterLevel water, SunLevel sun) {
        _profileManager.setProfile(water, sun);

        delete _hydrationCtrl;
        delete _thermalAdvisor;

        _hydrationCtrl =
            new HydrationController(
                _soilSensor,
                _waterActor,
                water
            );
        _thermalAdvisor =
            new ThermalAdvisor(
                _tempSensor,
                sun
            );
        _hydrationCtrl->setTestMode(_debugMode);
    }

    // ciclo automático normal
    void _runCycle() {
        bool soilOk = _soilSensor.read();
        bool tempOk = _tempSensor.read();
        if (!soilOk)
            Serial.println("Ciclo: error leyendo sensor de suelo");
        if (!tempOk)
            Serial.println("Ciclo: error leyendo DHT22");
        _thermalAdvisor->update();
        _hydrationCtrl->update();

        // publicar alerta MQTT si hay anomalía activa
        if (_network && _network->isConnected()) {
            if (_hydrationCtrl->hasAnomaly()) {
                _network->publishAlert(
                    "suelo",
                    "Caida brusca de humedad detectada"
                );
            }
            if (_thermalAdvisor->hasAnomaly()) {
                _network->publishAlert(
                    "temperatura",
                    "Temperatura alta sostenida — revisar ventilacion"
                );
            }
        }
        if (VERBOSE_LOG)
            _printStatus();
    }

    // status completo con advertencias contextuales
    void _printStatus() {
        float temp    = _tempSensor.getTemperature();
        float humAire = _tempSensor.getHumidity();
        float soil    = _soilSensor.getMoisturePercent();
        float tMin = _thermalAdvisor->getTempMin();
        float tMax = _thermalAdvisor->getTempMax();
        float sMin = _hydrationCtrl->getSoilActivateAt();
        float sMax = _hydrationCtrl->getSoilDeactivateAt();
        bool tempAlta = !isnan(temp) && temp > tMax;
        bool tempBaja = !isnan(temp) && temp < tMin;
        bool sueloSeco = soil >= 0 && soil < sMin;
        Serial.println();
        Serial.println("Status");
        // temperatura con advertencia inline
        Serial.printf(
            "  temp aire    : %.1f°C  (deseado %.0f-%.0f°C)",
            isnan(temp) ? 0.0f : temp,
            tMin,
            tMax
        );

        if (tempAlta)
            Serial.print("  [!] ALTA — hace falta ventilacion!");
        else if (tempBaja)
            Serial.print("  [!] BAJA — enciende el foco");
        Serial.println();
        // humedad aire
        Serial.printf(
            "  hum aire     : %.1f%%  (rango %.0f-%.0f%%)\n",
            isnan(humAire) ? 0.0f : humAire,
            _thermalAdvisor->getHumMin(),
            _thermalAdvisor->getHumMax()
        );
        // humedad suelo con advertencia inline
        Serial.printf(
            "  hum suelo    : %.1f%%  (regar <%.0f%%, parar >%.0f%%)",
            soil < 0 ? 0.0f : soil,
            sMin,
            sMax
        );
        if (sueloSeco)
            Serial.print("  [!] SECO — necesita riego!");
        Serial.println();
        // actuadores y perfil
        Serial.printf(
            "  bomba        : %s\n",
            _waterActor.isActive() ? "ON" : "OFF"
        );
        Serial.printf(
            "  foco sug.    : %s\n",
            _thermalAdvisor->focoSugeridoTexto()
        );

        Serial.printf(
            "  perfil       : agua=%-5s sol=%-5s nivel=%d\n",
            ProfileManager::waterLevelName(_profileManager.getWaterLevel()),
            ProfileManager::sunLevelName(_profileManager.getSunLevel()),
            _profileManager.getSensibilityLevel()
        );

        // flags de anomalía activos
        if (_hydrationCtrl->hasAnomaly())
            Serial.println("  [!] anomalia: caida brusca de humedad en suelo detectada");
        if (_thermalAdvisor->hasAnomaly())
            Serial.println("  [!] anomalia: temperatura alta sostenida — revisar ventilacion");
        Serial.println();
    }

    // imprime solo los datos crudos de sensores según el modo
    void _printReview() {
        float temp    = _tempSensor.getTemperature();
        float humAire = _tempSensor.getHumidity();
        float soil    = _soilSensor.getMoisturePercent();

        switch (_reviewMode) {
            case ReviewMode::ALL:
                // STATUS HIGH: todos los sensores, sin recomendaciones
                Serial.printf(
                    "Sensors: temp=%.1f°C | hum_aire=%.1f%% | hum_suelo=%.1f%%\n",
                    isnan(temp) ? 0.0f : temp,
                    isnan(humAire) ? 0.0f : humAire,
                    soil < 0 ? 0.0f : soil
                );
                break;
            case ReviewMode::TEMP: {
                bool alta =
                    !isnan(temp)
                    && temp > _thermalAdvisor->getTempMax();
                bool baja =
                    !isnan(temp)
                    && temp < _thermalAdvisor->getTempMin();
                Serial.printf(
                    "[REVIEW] Temp Aire -> Deseas: %.0f-%.0f°C | Tienes: %.1f°C%s\n",
                    _thermalAdvisor->getTempMin(),
                    _thermalAdvisor->getTempMax(),
                    isnan(temp) ? 0.0f : temp,
                    alta ? " (Hace falta ventilacion!)" :
                    baja ? " (Temperatura baja, enciende el foco)" : ""
                );
                break;
            }
            case ReviewMode::HUM:
                Serial.printf(
                    "[REVIEW] Hum Aire  -> Deseas: %.0f-%.0f%% | Tienes: %.1f%%\n",
                    _thermalAdvisor->getHumMin(),
                    _thermalAdvisor->getHumMax(),
                    isnan(humAire) ? 0.0f : humAire
                );
                break;
            case ReviewMode::SOIL: {
                bool seco =
                    soil >= 0
                    && soil < _hydrationCtrl->getSoilActivateAt();
                Serial.printf(
                    "[REVIEW] Hum Suelo -> Deseas: >%.0f%% | Tienes: %.1f%%%s\n",
                    _hydrationCtrl->getSoilActivateAt(),
                    soil < 0 ? 0.0f : soil,
                    seco ? " (Necesita riego!)" : ""
                );
                break;
            }
            default:
                break;
        }
    }

    // Debug completa — se imprime al arrancar y con cualquier cosa q no sea un comando
    void _printHelp() {
        Serial.println();
        Serial.println(
            "\nHELP - Comandos:\n"
            "  STATUS          estado completo\n"
            "  STATUS HIGH     bucle datos crudos cada 2s\n"
            "  REVIEW TEMP     bucle temp aire\n"
            "  REVIEW HUM      bucle hum aire\n"
            "  REVIEW SOIL     bucle hum suelo\n"
            "  PROFILE         ver perfil activo\n"
            "  ANOMALY CLEAR   resetea alertas\n"
            "  TEST ON/OFF     modo debug\n"
            "  PUMP ON/OFF     bomba manual (TEST ON)\n"
            "  PROFILE H M     cambia perfil (TEST ON)\n"
            "  HELP            esta lista\n"
        );
    }

    void _handleCommand(const String& raw) {
        // cualquier comando cancela el review o typeo q no reconozca lol
        if (_reviewMode != ReviewMode::NONE) {
            _reviewMode = ReviewMode::NONE;
            Serial.println("[review] detenido");
            // si el comando era solo para salir, no seguir procesando
            if (raw.equalsIgnoreCase("STOP")
                || raw.equalsIgnoreCase("EXIT")) {
                return;
            }
        }

        // STATUS - Estado general del sistema
        if (raw.equalsIgnoreCase("STATUS")) {
            _soilSensor.read();
            _tempSensor.read();
            _thermalAdvisor->update();

            _printStatus();
            return;
        }

        // STATUS HIGH — Bucle de datos
        if (raw.equalsIgnoreCase("STATUS HIGH")) {
            _reviewMode   = ReviewMode::ALL;
            _lastReviewMs = 0;
            Serial.println(
                "Sensors: iniciando bucle cada 2s — escribe cualquier cosa para salir"
            );
            return;
        }

        // REVIEW - Un STATUS pero de un sensor en especifico
        //Humedad en el Aire
        if (raw.equalsIgnoreCase("REVIEW TEMP")) {
            _reviewMode = ReviewMode::TEMP;
            _lastReviewMs = 0;
            Serial.println(
                "Review: temperatura del aire cada 2.5s — escribe algo para salir"
            );
            return;
        }
        //Humedad en el suelo
        if (raw.equalsIgnoreCase("REVIEW HUM")) {
            _reviewMode = ReviewMode::HUM;
            _lastReviewMs = 0;
            Serial.println(
                "Review: humedad del aire cada 2.5s — escribe algo para salir"
            );
            return;
        }
        //Temperatura en el Aire
        if (raw.equalsIgnoreCase("REVIEW SOIL")) {
            _reviewMode = ReviewMode::SOIL;
            _lastReviewMs = 0;
            Serial.println(
                "Review: humedad del suelo cada 2.5s — escribe algo para salir"
            );
            return;
        }

        // PROFILE sin parámetros — siempre disponible
        if (raw.equalsIgnoreCase("PROFILE")) {
            Serial.println("Profile: perfil activo:");
            _profileManager.printDebug();
            Serial.printf(
                "  suelo: bomba ON si < %.0f%% | bomba OFF si >= %.0f%%\n",
                _hydrationCtrl->getSoilActivateAt(),
                _hydrationCtrl->getSoilDeactivateAt()
            );
            Serial.printf(
                "  aire:  temp deseada %.0f-%.0f°C | hum %.0f-%.0f%%\n",
                _thermalAdvisor->getTempMin(),
                _thermalAdvisor->getTempMax(),
                _thermalAdvisor->getHumMin(),
                _thermalAdvisor->getHumMax()
            );
            if (!_debugMode)
                Serial.println("  (activa TEST ON para cambiar el perfil)");
            return;
        }
        // PROFILE con parámetros — solo en debug
        if (raw.startsWith("PROFILE ")) {
            if (!_debugMode) {
                Serial.println(
                    "Profile: activa TEST ON primero para cambiar perfil"
                );
                return;
            }
            _handleProfileCmd(raw);
            return;
        }
        // ANOMALY CLEAR — El mismo ahi explica lo q hace
        if (raw.equalsIgnoreCase("ANOMALY CLEAR")) {
            bool hadAnomaly =
                _hydrationCtrl->hasAnomaly()
                || _thermalAdvisor->hasAnomaly();
            _hydrationCtrl->clearAnomaly();
            _thermalAdvisor->clearAnomaly();
            if (hadAnomaly)
                Serial.println(
                    "Anomaly: flags reseteados — el sistema vuelve a monitorear normalmente"
                );
            else
                Serial.println(
                    "Anomaly: no habia anomalias activas"
                );
            return;
        }
        // TEST ON
        if (raw.equalsIgnoreCase("TEST ON")) {
            _debugMode = true;
            _hydrationCtrl->setTestMode(true);
            Serial.println(
                "[Debug] modo debug ON — ciclo automatico pausado"
            );
            Serial.println(
                "[Debug] nuevos comandos: PUMP ON | PUMP OFF | PROFILE H M | TEST OFF"
            );
            return;
        }
        // HELP siempre disponible
        if (raw.equalsIgnoreCase("HELP")) {
            _printHelp();
            return;
        }

        // Comandos exclusivos de modo debug
        if (!_debugMode) {
            Serial.println(
                "Sistema: comando no reconocido. escribe HELP para ver todos."
            );
            return;
        }

        if (raw.equalsIgnoreCase("TEST OFF")) {
            _debugMode = false;
            _hydrationCtrl->setTestMode(false);

            Serial.println(
                "[Debug] modo debug OFF — control automatico restaurado"
            );
            return;
        }

        if (raw.equalsIgnoreCase("PUMP ON")
            || raw.equalsIgnoreCase("PUMP OFF")) {
            _hydrationCtrl->handleSerialCommand(raw);

            return;
        }
        Serial.println(
            "[Debug] comando no reconocido. escribe HELP para ver todos."
        );
    }

    //Configuracion del perfil manualmente
    void _handleProfileCmd(const String& raw) {
        String args = raw.substring(8);

        args.trim();
        args.toUpperCase();

        if (args.length() < 3) {
            Serial.println(
                "Profile: Como usar? -> PROFILE <agua:L|M|H> <sol:L|M|H>  Ej: PROFILE H M"
            );
            return;
        }

        WaterLevel w;
        SunLevel   s;
        switch (args.charAt(0)) {
            case 'L':
                w = WaterLevel::WATER_LOW;
                break;
            case 'M':
                w = WaterLevel::WATER_MID;
                break;
            case 'H':
                w = WaterLevel::WATER_HIGH;
                break;
            default:
                Serial.println("[profile] agua invalida: L M o H");
                return;
        }
        switch (args.charAt(2)) {
            case 'L':
                s = SunLevel::SUN_LOW;
                break;
            case 'M':
                s = SunLevel::SUN_MID;
                break;
            case 'H':
                s = SunLevel::SUN_HIGH;
                break;
            default:
                Serial.println("[profile] sol invalido: L M o H");
                return;
        }
        _applyProfile(w, s);
        Serial.printf(
            "Profile: aplicado — nivel %d — intervalo %lus\n",
            _profileManager.getSensibilityLevel(),
            _profileManager.getSampleIntervalMs() / 1000UL
        );
    }
};

static CoreEngine engine;
void setup() {
    engine.begin();
}
void loop() {
    engine.loop();
}
//Tu puedes ver como gradualmente se me acaba la energia a medida que voy documentando.
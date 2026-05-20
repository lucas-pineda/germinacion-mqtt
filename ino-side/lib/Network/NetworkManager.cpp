#include "NetworkManager.h"
#include "SoilSensor.h"
#include "TempSensor.h"
#include "WaterActor.h"
#include "HeatActor.h"
#include "ProfileManager.h"

static const char* TOPIC_TELEMETRY = "germinador/telemetria";
static const char* TOPIC_COMMANDS  = "germinador/comandos";
static const char* TOPIC_ALERTS    = "germinador/alertas";
static const char* MQTT_CLIENT_ID  = "esp32-germinador";

NetworkManager* NetworkManager::_instance = nullptr;

NetworkManager::NetworkManager(
    const char*  ssid,
    const char*  password,
    const char*  brokerIp,
    uint16_t     brokerPort,
    SoilSensor&  soil,
    TempSensor&  temp,
    WaterActor&  pump,
    HeatActor&   lamp,
    ProfileManager& profile
)
    : _ssid(ssid), _password(password),
      _brokerIp(brokerIp), _brokerPort(brokerPort),
      _soil(soil), _temp(temp), _pump(pump), _lamp(lamp), _profile(profile),
      _mqtt(_wifiClient),
      _lastTelemetryMs(0),
      _lastReconnectMs(0)
{
    _instance = this;
}

void NetworkManager::begin() {
    _mqtt.setServer(_brokerIp, _brokerPort);
    _mqtt.setCallback(_mqttCallback);
    _mqtt.setBufferSize(512);

    _connectWifi();
    if (WiFi.status() == WL_CONNECTED) _connectMqtt();
}

void NetworkManager::update() {
    // reconexión no bloqueante — no usa while ni delay
    if (!_mqtt.connected()) {
        unsigned long now = millis();
        if (now - _lastReconnectMs >= RECONNECT_INTERVAL_MS) {
            _lastReconnectMs = now;
            if (WiFi.status() != WL_CONNECTED) _connectWifi();
            if (WiFi.status() == WL_CONNECTED)  _connectMqtt();
        }
        return; // si no hay conexión, no intentar publicar
    }

    _mqtt.loop();

    unsigned long now = millis();
    if (now - _lastTelemetryMs >= TELEMETRY_INTERVAL_MS) {
        _lastTelemetryMs = now;
        publishTelemetry();
    }
}

bool NetworkManager::isConnected() {
    return _mqtt.connected();
}

void NetworkManager::setCommandCallback(std::function<void(const String&)> cb) {
    _commandCallback = cb;
}

// Wi-Fi: intento único con timeout de 8s — no bloqueante en el loop
void NetworkManager::_connectWifi() {
    if (WiFi.status() == WL_CONNECTED) return;

    Serial.printf("Network: conectando a '%s'...\n", _ssid);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);

    // diagnóstico: escanear redes antes de conectar
    int n = WiFi.scanNetworks();
    if (n <= 0) {
        Serial.println("Network: no se encontraron redes — verifica antena");
    } else {
        Serial.printf("Network: %d red(es) visible(s):\n", n);
        for (int i = 0; i < n; i++) {
            Serial.printf("  [%d] %-30s  %d dBm  canal %d\n",
                          i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
        }
    }
    WiFi.scanDelete();

    WiFi.begin(_ssid, _password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 8000UL) {
        delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("Network: Wi-Fi OK — IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("Network: Wi-Fi falló — reintento en el loop");
    }
}

void NetworkManager::_connectMqtt() {
    if (_mqtt.connected()) return;

    Serial.printf("Network: conectando a broker %s:%d...\n", _brokerIp, _brokerPort);

    if (_mqtt.connect(MQTT_CLIENT_ID)) {
        _mqtt.subscribe(TOPIC_COMMANDS);
        Serial.printf("Network: broker OK — escuchando '%s'\n", TOPIC_COMMANDS);
    } else {
        Serial.printf("Network: broker falló rc=%d — reintento en %lus\n",
                      _mqtt.state(), RECONNECT_INTERVAL_MS / 1000UL);
    }
}

// publica telemetría con lecturas actuales de sensores
void NetworkManager::publishTelemetry() {
    if (!_mqtt.connected()) return;

    _soil.read();
    _temp.read();

    StaticJsonDocument<256> doc;
    doc["ts"]          = millis();
    doc["temp_aire"]   = _temp.isValid()  ? _temp.getTemperature()    : -1.0f;
    doc["hum_aire"]    = _temp.isValid()  ? _temp.getHumidity()       : -1.0f;
    doc["hum_suelo"]   = _soil.isValid()  ? _soil.getMoisturePercent(): -1.0f;
    doc["bomba"]       = _pump.isActive();
    doc["nivel"]       = _profile.getSensibilityLevel();
    doc["agua"]        = ProfileManager::waterLevelName(_profile.getWaterLevel());
    doc["sol"]         = ProfileManager::sunLevelName(_profile.getSunLevel());

    char buffer[256];
    size_t n = serializeJson(doc, buffer);
    _mqtt.publish(TOPIC_TELEMETRY, buffer, n);

    Serial.printf("Network: telemetria → %s\n", buffer);
}

// publica alerta crítica — llamado desde CoreEngine cuando hay anomalía
void NetworkManager::publishAlert(const char* tipo, const char* mensaje) {
    if (!_mqtt.connected()) return;

    StaticJsonDocument<128> doc;
    doc["ts"]      = millis();
    doc["tipo"]    = tipo;
    doc["mensaje"] = mensaje;

    char buffer[128];
    size_t n = serializeJson(doc, buffer);
    _mqtt.publish(TOPIC_ALERTS, buffer, n);

    Serial.printf("Network: alerta → %s\n", buffer);
}

// recibe payload de germinador/comandos y lo reenvía a CoreEngine
void NetworkManager::_handleIncomingCommand(const byte* payload, unsigned int length) {
    String cmd = "";
    for (unsigned int i = 0; i < length; i++) {
        cmd += (char)payload[i];
    }
    cmd.trim();
    Serial.printf("Network: comando recibido via MQTT: '%s'\n", cmd.c_str());

    if (_commandCallback) {
        _commandCallback(cmd);
    } else {
        Serial.println("Network: sin callback registrado — comando ignorado");
    }
}

void NetworkManager::_mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance == nullptr) return;

    if (strcmp(topic, TOPIC_COMMANDS) == 0) {
        _instance->_handleIncomingCommand(payload, length);
    }
}
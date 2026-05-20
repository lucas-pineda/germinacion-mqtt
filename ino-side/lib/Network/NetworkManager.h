#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

// módulo de conectividad Wi-Fi + MQTT
// tópicos:
//   germinador/telemetria  — publica lecturas de sensores cada ciclo
//   germinador/comandos    — escucha comandos de texto (se inyectan a CoreEngine)
//   germinador/alertas     — publica anomalías y avisos críticos

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <functional>

class SoilSensor;
class TempSensor;
class WaterActor;
class HeatActor;
class ProfileManager;

#define TELEMETRY_INTERVAL_MS   10000UL
#define RECONNECT_INTERVAL_MS   5000UL

class NetworkManager {
public:
    NetworkManager(
        const char*  ssid,
        const char*  password,
        const char*  brokerIp,
        uint16_t     brokerPort,
        SoilSensor&  soil,
        TempSensor&  temp,
        WaterActor&  pump,
        HeatActor&   lamp,
        ProfileManager& profile
    );

    // llama una vez en setup(), después de _applyProfile()
    void begin();
    // llama en cada iteración de loop() — no bloqueante
    void update();
    bool isConnected();
    // registra el callback que reenvía comandos MQTT a CoreEngine::handleCommand()
    void setCommandCallback(std::function<void(const String&)> cb);
    // publica una alerta crítica — llamar desde _runCycle() cuando hay anomalía
    void publishAlert(const char* tipo, const char* mensaje);
    // publica telemetría manualmente — útil si quieres forzarla desde _runCycle()
    void publishTelemetry();

private:
    const char*  _ssid;
    const char*  _password;
    const char*  _brokerIp;
    uint16_t     _brokerPort;
    SoilSensor&     _soil;
    TempSensor&     _temp;
    WaterActor&     _pump;
    HeatActor&      _lamp;
    ProfileManager& _profile;
    WiFiClient   _wifiClient;
    PubSubClient _mqtt;
    unsigned long _lastTelemetryMs;
    unsigned long _lastReconnectMs;

    std::function<void(const String&)> _commandCallback;
    void _connectWifi();
    void _connectMqtt();
    void _handleIncomingCommand(const byte* payload, unsigned int length);
    static void _mqttCallback(char* topic, byte* payload, unsigned int length);
    static NetworkManager* _instance;
};

#endif // NETWORK_MANAGER_H
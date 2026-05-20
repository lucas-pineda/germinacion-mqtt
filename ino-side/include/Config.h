/*
FILE: include/Config.h
Este archivo contiene ÚNICAMENTE macros #define y constantes globales.
Sirve para ahorrar un montón de memoria RAM en la ESP32
Aca solo se puede modificar la Confid WIFI (En caso de que no )
*/

#ifndef CONFIG_H
#define CONFIG_H

// pines de hardware — PCB germinador v1
#define PIN_DHT22               4       // GPIO4  — DHT22 temperatura y humedad del aire
#define PIN_SOIL_MOISTURE       34      // GPIO34 — sensor capacitivo de humedad de suelo
                                        //          si falla, revisar conexión en GPIO14 (D14 del esquemático)
#define PIN_RELAY_PUMP          5       // GPIO5  — bomba de agua (active-low)
#define PIN_RELAY_LAMP          18      // GPIO18 — relé foco (reservado, control manual)
#define DHT_TYPE                DHT22

/*  Sensor de las Actuadres (RELE)
Define qué patitas de la ESP32 controlarán los interruptores electrónicos
Para prender la bomba, el microcontrolador debe enviar un LOW (0 voltios)
y para apagarlos, un HIGH (3.3 voltios).
*/
#define RELAY_ON                LOW
#define RELAY_OFF               HIGH

/*  Calibración del Conversor Análogo-Digital (ADC)
Mide los extremos del sensor de humedad. 
SOIL_ADC_DRY es el valor de voltaje (en bits) cuando el sensor está seco al aire y SOIL_ADC_WET cuando está en agua pura. 
El SOIL_OVERSAMPLE 10 significa que el ESP32 tomará 10 lecturas seguidas muy rápido y sacará un promedio 
para eliminar el "ruido" y q no se sature
*/
// ajustar estos valores con el sensor en aire (seco) y sumergido en agua (húmedo)
#define SOIL_ADC_DRY            3200    // lectura en aire — completamente seco
#define SOIL_ADC_WET            1200    // lectura en agua — completamente húmedo
#define SOIL_OVERSAMPLE         10      // muestras promediadas por lectura

/*  Intervalos (Tiempo de Espera)
Define los tiempos límite basados en tu matriz de sensibilidad de 1 a 9. Si la planta está en nivel 9 (mucha agua, mucho sol), el sistema revisa los sensores rápido (cada 5 minutos). Si está en nivel 1, se duerme y revisa cada 30 minutos para ahorrar recursos. El UL al final le dice a C++ que es un número tipo Unsigned Long (entero enorme positivo), necesario porque el tiempo en milisegundos crece muy rápido.
*/

//Cambia temporalmente estos valores para tests 5000UL (5 segs), 15000UL (15 Segs)
#define SAMPLE_INTERVAL_MIN_MS  300000UL    // 300000 - 5 min  (nivel 9)
#define SAMPLE_INTERVAL_MAX_MS  1800000UL   // 1800000 - 30 min (nivel 1)

/*  WIFI
red a la que se conecta la ESP32 para comunicarse con a API/Base de datos
(Esta es la parte q dije q se puede modificar)
*/
#define WIFI_SSID               "FrElokeInvi"
#define WIFI_PASSWORD           "14213100v_OwO?"
#define MQTT_BROKER_IP          "192.168.1.58"
#define MQTT_BROKER_PORT         1883
#define MQTT_TOPIC_TELEMETRY    "germinador/telemetria"
#define MQTT_TOPIC_CMD          "germinador/comandos"

/* Consola Serial y Registros
SERIAL_BAUD_RATE define la velocidad de comunicación por cable entre la ESP32 y la laptop (115,200 pulsos por segundo). 
VERBOSE_LOG true activa el modo "parlero" del sistema
(Imprimirá en consola todo lo que pase en segundo plano)
*/
#define SERIAL_BAUD_RATE        115200
#define VERBOSE_LOG             true

// firmware
#define FW_VERSION              "3.1.0"
#define PROJECT_NAME            "AutoGerminator"

#endif // CONFIG_H
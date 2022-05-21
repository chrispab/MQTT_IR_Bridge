#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <assert.h>
// #include <AsyncElegantOTA.h>
// #include <AsyncTCP.h>
// #include <ESPAsyncWebServer.h>
#include <IRsend.h>
#include <PubSubClient.h>

// req for ota
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiUdp.h>
//
#include "LedFader.h"
#include "MQTTLib.h"
#include "WiFiLib.h"
#include "config.h"
#include "ircodes.h"

#define GREEN_LED_PIN GPIO_NUM_12   //
#define IR_LED_PIN GPIO_NUM_13      //
#define ONBOARD_LED_PIN GPIO_NUM_2  //
#define RX_PIN GPIO_NUM_14          //

const uint16_t kIrLed = IR_LED_PIN;  // ESP8266 GPIO pin to use.
IRsend irsend(kIrLed);               // Set the GPIO to be used to sending the message.

#define HEART_BEAT_TIME 800
#define BLUE_BEAT_TIME 300

LedFader heartBeatLED(GREEN_LED_PIN, 1, 0, 255, HEART_BEAT_TIME);
LedFader blueBeatLED(ONBOARD_LED_PIN, 2, 0, 50, BLUE_BEAT_TIME);

// raw codes
uint16_t rawData[75] = {
    2004, 988, 1002, 986, 1000, 986, 1000, 492, 498, 986, 502, 490, 1000,
    490, 502, 984, 502, 490, 1000, 986, 1000, 986, 500, 48852, 2002, 988,
    1000, 986, 1000, 986, 502, 490, 1000, 984, 504, 490, 1000, 490, 502,
    986, 500, 490, 502, 490, 502, 490, 1000, 986, 500, 48416, 2002, 988,
    1000, 988, 998, 984, 1002, 986, 500, 492, 500, 490, 1000, 490, 500,
    984, 504, 492, 500, 492, 500, 490, 1000, 986, 500};
// length = 75 ^ above raw array
uint16_t rawDataLength = 75;

// accepts pointer to command string
// returns thge code
uint64_t lookupCommandCode(const char *commandName) {
    typedef struct item_t {
        const char *commandStr;
        uint64_t code;
    } item_t;
    item_t table[] = {
        {"power_on", POWER_ON},
        {"power_off", POWER_OFF},
        {"cd", CD},
        {"tuner", TUNER},
        {"aux", AUX},
        {"disc", DISC},
        {"tape1_mon", TAPE_1_MON},
        {"tape2", TAPE_2},
        {"video1", VIDEO1},
        {"video2", VIDEO2},
        {"video3", VIDEO3},
        {"volume_up", VOLUME_UP},
        {"volume_down", VOLUME_DOWN},
        {"mute", MUTE},
        {NULL, 0}};

    for (item_t *p = table; p->commandStr != NULL; ++p) {
        if (strcmp(p->commandStr, commandName) == 0) {
            return p->code;
        }
    }
    return -EINVAL;
}

// pass in topic string and
// return pointer to final token - command string
char *getCommandStr(char *topic, char *resultCommandStr) {
    // char *lastToken = NULL;
    char *token = strtok(topic, "/");
    while (token) {
        Serial.printf("token: %s\n", token);
        // copy token string
        strcpy(resultCommandStr, token);
        // lastToken = token;
        token = strtok(NULL, "/");
    }
    return 0;
}

void pulseLED(int pulses, int time) {
    for (size_t i = 0; i < pulses; i++) {
        heartBeatLED.fullOff();
        delay(time);
        heartBeatLED.fullOn();
        delay(time/3);
    }
}

// MQTT stuff
void callback(char *topic, byte *payload, unsigned int length) {
    // handle message arrived
    // format and display the whole MQTT message and payload
    char fullMQTTmessage[255];  // = "MQTT rxed thisisthetopicforthismesage and
                                // finally the payload, and a bit extra to make
                                // sure there is room in the string and even
                                // more chars";
    strcpy(fullMQTTmessage, "MQTT Rxed Topic: [");
    strcat(fullMQTTmessage, topic);
    strcat(fullMQTTmessage, "], ");
    // append payload and add \o terminator
    strcat(fullMQTTmessage, "Payload: [");
    strncat(fullMQTTmessage, (char *)payload, length);
    strcat(fullMQTTmessage, "]");

    Serial.println(fullMQTTmessage);

    // assume filtered for 'irbridge/amplifier/' already
    // get last part of string - the command,
    // then irsend the code version of the command -m use a struct?
    //  https://stackoverflow.com/questions/5193570/value-lookup-table-in-c-by-strings
    char commandStr[25];
    getCommandStr(topic, commandStr);  // get pointer to the command string 3rd section
    uint64_t command = lookupCommandCode(commandStr);

    Serial.printf("command: %#08x\n", command);

    if ( (command != -EINVAL) && ((payload[1]) == 'n') ) {//and if payload = o'n', dont do anything with OFF
        // good command code so txmit
        Serial.printf("command txed: %#08x\n", command);

        irsend.sendNEC(command);
        pulseLED(4,50);

    }
    //         // irsend.sendNEC(POWER_ON);
    //! possible incoming topics and payload: "irbridge/amplifier/standby"     "on|off"
    else if (strcmp(topic, "irbridge/amplifier/code") == 0) {  // raw code
        Serial.print("plain NEC code Tx : ");
        unsigned long actualval;
        actualval = strtoul((char *)payload, NULL, 10);
        Serial.println(actualval);
        irsend.sendNEC(actualval);
    }
    //     else if (strcmp(topic, "irbridge/amplifier/raw") == 0) {  // raw code
    //         Serial.print("raw code Tx : ");
    //         unsigned long actualval;
    //         actualval = strtoul((char *)payload, NULL, 10);
    //         Serial.println(actualval);
    //         irsend.sendRaw(rawData, rawDataLength, 38);  // Send a raw data capture at 38kHz.
    //     }
}

IPAddress mqttBroker(192, 168, 0, MQTT_LAST_OCTET);
WiFiClient myWiFiClient;
PubSubClient MQTTclient(mqttBroker, 1883, callback, myWiFiClient);

const uint32_t kBaudRate = 115200;

void setup() {
    Serial.begin(115200);
    delay(3000);

    Serial.println("Hi from irbridge ... Booting");
    connectWiFi();
    printWifiStatus();
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.flush();

    delay(5000);
    // connectMQTT();
    reconnectMQTT();

    irsend.begin();
    heartBeatLED.begin();  // initialize
    blueBeatLED.begin();   // initialize
    // you're connected now, so print out the status:

    // Serial.begin(kBaudRate, SERIAL_8N1);
    // while (!Serial)  // Wait for the serial connection to be establised.
    //   delay(50);

    /* we use mDNS instead of IP of ESP32 directly */
    // hostname.local
    ArduinoOTA.setHostname("irbridge");

    ArduinoOTA
        .onStart([]() {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH)
                type = "sketch";
            else  // U_SPIFFS
                type = "filesystem";

            // NOTE: if updating SPIFFS this would be the place to unmount
            // SPIFFS using SPIFFS.end()
            Serial.println("Start updating " + type);
        })
        .onEnd([]() { Serial.println("\nEnd"); })
        .onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        })
        .onError([](ota_error_t error) {
            Serial.printf("Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR)
                Serial.println("Auth Failed");
            else if (error == OTA_BEGIN_ERROR)
                Serial.println("Begin Failed");
            else if (error == OTA_CONNECT_ERROR)
                Serial.println("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR)
                Serial.println("Receive Failed");
            else if (error == OTA_END_ERROR)
                Serial.println("End Failed");
        });

    ArduinoOTA.begin();

    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void loop() {
    // connectWiFi();
    // maybe checkwifi here
    connectWiFi();

    if (!MQTTclient.connected()) {
        // Attempt to reconnect
        reconnectMQTT();  // Attempt to reconnect
    } else {
        // Client is connected
        MQTTclient.loop();  // process any MQTT stuff, returned in callback
    }

    ArduinoOTA.handle();

    heartBeatLED.update();
    blueBeatLED.update();

    //! send out telemetry every 5 mins
    publishTelemetry();
}

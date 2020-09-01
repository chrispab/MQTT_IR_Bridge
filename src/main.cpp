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

    if (strcmp(topic, "irbridge/amplifier/video1") == 0) {  // does topic match this text?
        Serial.println("ir send amp source select video1");
        irsend.sendNEC(SELECT_VIDEO1);
    } else if (strcmp(topic, "irbridge/amplifier/tuner") == 0) {  // does topic match this text?
        Serial.println("ir send amp source select tuner");
        irsend.sendNEC(SELECT_TUNER);
    } else if (strcmp(topic, "irbridge/amplifier/aux") == 0) {  // does topic match this text?
        Serial.println("ir send amp source select AUX");
        irsend.sendNEC(SELECT_AUX);
    } else if (strcmp(topic, "irbridge/amplifier/mute") == 0) {  // does topic match this text?
        Serial.println("ir send amp mute");
        irsend.sendNEC(MUTE);
    } else if (strcmp(topic, "irbridge/amplifier/volumeup") == 0) {  // does topic match this text?
        Serial.println("ir send amp vol up");
        irsend.sendNEC(VOLUME_UP);
    } else if (strcmp(topic, "irbridge/amplifier/volumedown") == 0) {  // does topic match this text?
        Serial.println("ir send amp vol down");
        irsend.sendNEC(VOLUME_DOWN);
    } else if (strcmp(topic, "irbridge/amplifier/standby") == 0) {  // does topic match this text?
        if ((payload[1] - 'n') == 0) {                              // found the 'n' in "on" ?
            Serial.println("ir send amp standby on");
            irsend.sendNEC(STANDBY_ON);
        } else {  // payload must have been "off"
            Serial.println("ir send amp standby off");
            irsend.sendNEC(STANDBY_OFF);
        }
    }
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

IPAddress mqttBroker(192, 168, 0, 200);
WiFiClient myWiFiClient;
PubSubClient MQTTclient(mqttBroker, 1883, callback, myWiFiClient);

const uint32_t kBaudRate = 115200;

void setup() {
    Serial.begin(115200);
    Serial.println("Booting");
    connectWiFi();
    printWifiStatus();
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.flush();

    delay(5000);
    connectMQTT();

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
    connectMQTT();
    ArduinoOTA.handle();

    heartBeatLED.update();
    blueBeatLED.update();

    MQTTclient.loop();  // process any MQTT stuff, returned in callback
}

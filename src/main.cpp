#include <Arduino.h>
#include <assert.h>
// #include <IRrecv.h>
#include <IRremoteESP8266.h>
// #include <IRac.h>
// #include <IRtext.h>
// #include <IRutils.h>
#include <AsyncElegantOTA.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <IRsend.h>
#include <WiFi.h>

#include "LedFader.h"
#include "MQTTLib.h"
#include "WiFiLib.h"

AsyncWebServer server(80);

#define RED_LED_PIN GPIO_NUM_12     //
#define IR_LED_PIN GPIO_NUM_13      //
#define ONBOARD_LED_PIN GPIO_NUM_2  //
#define RX_PIN GPIO_NUM_14          //

const uint16_t kIrLed =
    IR_LED_PIN;         // ESP8266 GPIO pin to use. Recommended: 4 (D2).
IRsend irsend(kIrLed);  // Set the GPIO to be used to sending the message.

#define HEART_BEAT_TIME 500
LedFader heartBeatLED(RED_LED_PIN, 1, 0, 255, HEART_BEAT_TIME, true);
// LedFader testBeatLED(IR_LED_PIN, 1, 0, 255, HEART_BEAT_TIME, true);

// MQTT stuff
#include <PubSubClient.h>
void callback(char *topic, byte *payload, unsigned int length) {
  // handle message arrived
  // format and display the whole MQTT message and payload
  char fullMQTTmessage[255];  // = "MQTT rxed thisisthetopicforthismesage and
                              // finally the payload, and a bit extra to make
                              // sure there is room in the string and even more
                              // chars";
  strcpy(fullMQTTmessage, "MQTT Rxed Topic: [");
  strcat(fullMQTTmessage, topic);
  strcat(fullMQTTmessage, "], ");
  // append payload and add \o terminator
  strcat(fullMQTTmessage, "Payload: [");
  strncat(fullMQTTmessage, (char *)payload, length);
  strcat(fullMQTTmessage, "]");

  // Serial.println(fullMQTTmessage);
  Serial.println(fullMQTTmessage);
  // only proces if topic starts with "IRBridge/cmnd/Power"

  //! possible incoming topics and payload:
  // "irbridge/amplifier/standby"     "on|off"
  if (strstr(topic, "irbridge/amplifier/code") != NULL) {  // raw code
    //convert payload 'chars to the actual hex value
    // if ((payload[1] - 'n') == 0) {  // found the 'n' in "on" ?
    Serial.print("raw code rx : ");
    unsigned long actualval;
    actualval = strtoul((char *)payload,    NULL, 10);
    Serial.println(actualval);
      irsend.sendNEC(actualval);
    // } else {  // payload must have been "off"
    //   Serial.println("ir send amp standby off");
    //   irsend.sendNEC(0xE13E13EC);
    }
    if (strstr(topic, "irbridge/amplifier/standby") !=
        NULL) {  // does topic match this text?
      // is it on or off? payload
      // if ((payload[0] - '1') == 0) {
      //   newState = 1;
      // }
      if ((payload[1] - 'n') == 0) {  // found the 'n' in "on" ?
        Serial.println("ir send amp standby on");
        irsend.sendNEC(0xE13EA45B);
        // delay(5000);
        // Serial.println("NEC off");
        // irsend.sendNEC(0xE13E13EC);
        // delay(5000);
      } else {  // payload must have been "off"
        Serial.println("ir send amp standby off");
        irsend.sendNEC(0xE13E13EC);
      }
    }
  }

  IPAddress mqttBroker(192, 168, 0, 200);
  WiFiClient WiFiEClient;
  PubSubClient MQTTclient(mqttBroker, 1883, callback, WiFiEClient);
  // PubSubClient client(WiFiEClient);

  const uint32_t kBaudRate = 115200;

  void setup() {
    heartBeatLED.begin();  // initialize
    ////testBeatLED.begin();                         // initialize
    connectWiFi();
    // you're connected now, so print out the status:
    printWifiStatus();
    connectMQTT();

    irsend.begin();

    Serial.begin(kBaudRate, SERIAL_8N1);
    while (!Serial)  // Wait for the serial connection to be establised.
      delay(50);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", "Hi! I am ESP8266.");
    });

    AsyncElegantOTA.begin(&server);  // Start ElegantOTA
    server.begin();
    Serial.println("HTTP server started");
  }

  // The repeating section of the code
  void loop() {
    AsyncElegantOTA.loop();

    heartBeatLED.update();

    connectMQTT();

    MQTTclient.loop();  // process any MQTT stuff, returned in callback
  }

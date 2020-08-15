#include <Arduino.h>
#include <assert.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRac.h>
#include <IRtext.h>
#include <IRutils.h>
#include <IRsend.h>

#include <WiFi.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

#include "WiFiLib.h"
#include "MQTTLib.h"

#include "LedFader.h"


AsyncWebServer server(80);


#define RED_LED_PIN GPIO_NUM_12     //LHS_P_3
#define IR_LED_PIN GPIO_NUM_13      //LHS_P_4
#define ONBOARD_LED_PIN GPIO_NUM_2      //

const uint16_t kIrLed = IR_LED_PIN; // ESP8266 GPIO pin to use. Recommended: 4 (D2).
IRsend irsend(kIrLed);              // Set the GPIO to be used to sending the message.

#define HEART_BEAT_TIME 500
LedFader heartBeatLED(RED_LED_PIN, 1, 0, 255, HEART_BEAT_TIME, true);
//LedFader testBeatLED(IR_LED_PIN, 1, 0, 255, HEART_BEAT_TIME, true);

// MQTT stuff
#include <PubSubClient.h>
void callback(char *topic, byte *payload, unsigned int length)
{
  // handle message arrived
  //format and display the whole MQTT message and payload
  char fullMQTTmessage[255]; // = "MQTT rxed thisisthetopicforthismesage and finally the payload, and a bit extra to make sure there is room in the string and even more chars";
  strcpy(fullMQTTmessage, "MQTT Rxed [");
  strcat(fullMQTTmessage, topic);
  strcat(fullMQTTmessage, "]:");
  // append payload and add \o terminator
  strcat(fullMQTTmessage, "[");
  strncat(fullMQTTmessage, (char *)payload, length);
  strcat(fullMQTTmessage, "]");

  // Serial.println(fullMQTTmessage);
  Serial.println(fullMQTTmessage);
  // only proces if topic starts with "IRBridge/cmnd/Power"
  
  //!possible incoming topics and payload:
  // "irbridge/amplifier/standby"     "on|off"
  if (strstr(topic, "irbridge/amplifier/standby") != NULL) //does topic match this text?
  {
    //is it on or off? payload
        // if ((payload[0] - '1') == 0) {
    //   newState = 1;
    // }
    if ((payload[1] - 'n') == 0) {//found the 'n' in "on" ?
      Serial.println("ir send amp standby on");
      irsend.sendNEC(0xE13EA45B);
      // delay(5000);
      // Serial.println("NEC off");
      // irsend.sendNEC(0xE13E13EC); 
      // delay(5000);
    } else { //payload must have been "off"
      Serial.println("ir send amp standby off");
      irsend.sendNEC(0xE13E13EC);       
    }
    // e.g incoming topic = "433Bridge/cmnd/Power1" to "...Power16", and payload
    // = 1 or 0 either match whole topic string or trim off last 1or 2 chars and
    // convert to a number, convert last 1-2 chars to socket number
    // char lastChar = topic[strlen(topic) - 1];       // lst char will always be a digit char
    // char lastButOneChar = topic[strlen(topic) - 2]; // see if last but 1 is also a digit char - ie number has two digits - 10 to 16

    //socketNumber = lastChar - '0'; //get actual numeric value
    // if ((lastButOneChar == '1'))
    // {                                   // it is a 2 digit number
      //socketNumber = socketNumber + 10; // calc actual int
    // }

    // if ((payload[0] - '1') == 0) {
    //   newState = 1;
    // }
    //   Serial.print("......payload[");
    //   for (int i=0;i<length;i++) {
    //     Serial.print((char)payload[i]);
    //   }
    //   Serial.println("]");
    //   uint8_t newState = 0;  // default to off

    //   if ((char)(payload[0]) == '1') {
    //     newState = 1;
    //   }

    //     Serial.print(",,,,,,,,,,,,,,,,,,,,,,,,,,[");

    //   Serial.print(newState);
    //   Serial.println("]");

    //   // signal a new command has been rxed and
    //   // topic and payload also available
    //   MQTTNewState = newState;          // 0 or 1
    //   MQTTSocketNumber = socketNumber;  // 1-16
    //   MQTTNewData = true;
    //   return;
    }
  }





  IPAddress mqttBroker(192, 168, 0, 200);
  WiFiClient WiFiEClient;
  PubSubClient MQTTclient(mqttBroker, 1883, callback, WiFiEClient);
  PubSubClient client(WiFiEClient);

  // ==================== start of TUNEABLE PARAMETERS ====================
  // An IR detector/demodulator is connected to GPIO pin 14
  // e.g. D5 on a NodeMCU board.
  // Note: GPIO 16 won't work on the ESP8266 as it does not have interrupts.
  const uint16_t kRecvPin = IR_LED_PIN;

  // The Serial connection baud rate.
  // i.e. Status message will be sent to the PC at this baud rate.
  // Try to avoid slow speeds like 9600, as you will miss messages and
  // cause other problems. 115200 (or faster) is recommended.
  // NOTE: Make sure you set your Serial Monitor to the same speed.
  const uint32_t kBaudRate = 115200;

  // As this program is a special purpose capture/decoder, let us use a larger
  // than normal buffer so we can handle Air Conditioner remote codes.
  const uint16_t kCaptureBufferSize = 1024;

#if DECODE_AC
  // Some A/C units have gaps in their protocols of ~40ms. e.g. Kelvinator
  // A value this large may swallow repeats of some protocols
  const uint8_t kTimeout = 50;
#else  // DECODE_AC
  // Suits most messages, while not swallowing many repeats.
  const uint8_t kTimeout = 15;
#endif // DECODE_AC
  const uint16_t kMinUnknownSize = 12;

#define LEGACY_TIMING_INFO false
  // ==================== end of TUNEABLE PARAMETERS ====================

  // Use turn on the save buffer feature for more complete capture coverage.
  IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, true);
  decode_results results; // Somewhere to store the results

  // This section of code runs only once at start-up.
  void setup() {
    heartBeatLED.begin(); // initialize
    ////testBeatLED.begin();                         // initialize
    connectWiFi();
    // you're connected now, so print out the status:
    printWifiStatus();
    connectMQTT();

    irsend.begin();

#if defined(ESP8266)
    Serial.begin(kBaudRate, SERIAL_8N1, SERIAL_TX_ONLY);
#else               // ESP8266
    Serial.begin(kBaudRate, SERIAL_8N1);
#endif              // ESP8266
    while (!Serial) // Wait for the serial connection to be establised.
      delay(50);
    // Perform a low level sanity checks that the compiler performs bit field
    // packing as we expect and Endianness is as we expect.
    // assert(irutils::lowLevelSanityCheck());

    Serial.printf("\n" D_STR_IRRECVDUMP_STARTUP "\n", kRecvPin);
#if DECODE_HASH
    // Ignore messages with less than minimum on or off pulses.
    irrecv.setUnknownThreshold(kMinUnknownSize);
#endif                   // DECODE_HASH
   // irrecv.enableIRIn(); // Start the receiver


     server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! I am ESP8266.");
  });

  AsyncElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  Serial.println("HTTP server started");

  }

  // The repeating section of the code
  void loop()
  {
      AsyncElegantOTA.loop();

    heartBeatLED.update(); // initialize
    //testBeatLED.update();  // initialize

    connectMQTT();

    MQTTclient.loop(); // process any MQTT stuff, returned in callback

    // Serial.println("NEC on");
    // irsend.sendNEC(0xE13EA45B);
    // delay(5000);
    // Serial.println("NEC off");
    // irsend.sendNEC(0xE13E13EC); // 12 bits & 2 repeats
    // delay(5000);
    // Serial.println("a rawData capture from IRrecvDumpV2");
    // irsend.sendRaw(rawData, 67, 38);  // Send a raw data capture at 38kHz.
    // delay(2000);
    // Serial.println("a Samsung A/C state from IRrecvDumpV2");
    // irsend.sendSamsungAC(samsungState);
    // delay(2000);
    //MQTTclient.publish("IRBridge/IM_HERE", "Online");//ensure send online
    //delay(5000);

    // Check if the IR code has been received.
    //   if (irrecv.decode(&results)) {
    //     // Display a crude timestamp.
    //     uint32_t now = millis();
    //     Serial.printf(D_STR_TIMESTAMP " : %06u.%03u\n", now / 1000, now % 1000);
    //     // Check if we got an IR message that was to big for our capture buffer.
    //     if (results.overflow)
    //       Serial.printf(D_WARN_BUFFERFULL "\n", kCaptureBufferSize);
    //     // Display the library version the message was captured with.
    //     Serial.println(D_STR_LIBRARY "   : v" _IRREMOTEESP8266_VERSION_ "\n");
    //     // Display the basic output of what we found.
    //     Serial.print(resultToHumanReadableBasic(&results));
    //     // Display any extra A/C info if we have it.
    //     String description = IRAcUtils::resultAcToString(&results);
    //     if (description.length()) Serial.println(D_STR_MESGDESC ": " + description);
    //     yield();  // Feed the WDT as the text output can take a while to print.
    // #if LEGACY_TIMING_INFO
    //     // Output legacy RAW timing info of the result.
    //     Serial.println(resultToTimingInfo(&results));
    //     yield();  // Feed the WDT (again)
    // #endif  // LEGACY_TIMING_INFO
    //     // Output the results as source code
    //     Serial.println(resultToSourceCode(&results));
    //     Serial.println();    // Blank line between entries
    // yield();             // Feed the WDT (again)
    // }
  }

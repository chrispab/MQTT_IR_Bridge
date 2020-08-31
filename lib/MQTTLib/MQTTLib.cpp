//#include "WebSocketLib.h"
#include "config.h"
#include <PubSubClient.h>
extern PubSubClient MQTTclient;

bool MQTTNewData = false;
int MQTTNewState = 0;      // 0 or 1
int MQTTSocketNumber = 1;  // 1-16

// MQTT stuff
// IPAddress mqttBroker(192, 168, 0, 200);
char subscribeTopic[] = "irbridge/#";
// char subscribeTopic2[] = "irbridge/amplifier/code";
// char subscribeTopic3[] = "Zone3/#";
// char subscribeTopic[] = "#";

char publishTempTopic[] = "433Bridge/Temperature";
char publishHumiTopic[] = "433Bridge/Humidity";

/**
 * 
 *
 * 
*/ 
void connectMQTT() {
  bool MQTTConnectTimeout = false;
  unsigned long checkPeriodMillis = 20000;
  unsigned long timeOutMillis = 5000;
  unsigned long now;
  unsigned long nowMillis = millis();
  static unsigned long lastReconnectAttemptMillis =
      nowMillis - checkPeriodMillis - 1000;

  // myWebSerial.println("HELLO...");
  // Serial.print("nowMillis : ");
  // Serial.println(nowMillis);
  // Serial.println("Last reconn attempt : ", lastReconnectAttemptMillis);
  // myWebSerial.println("checkPeriodMillis : ", checkPeriodMillis);

  if ((nowMillis - lastReconnectAttemptMillis) > checkPeriodMillis) {
    // myWebSerial.println("ready to try MQTT reconnectMQTT...");
    Serial.print("nowMillis : ");
    Serial.println(nowMillis);
    // Serial.print("lastReconnectAttemptMillis : ");
    // Serial.println(lastReconnectAttemptMillis);
    // Serial.print("checkPeriodMillis : ");
    // Serial.println(checkPeriodMillis);
    Serial.println("Checking if MQTT needs reconnect");

    // if (!MQTTclient.connected()) {
      MQTTConnectTimeout = false;
    while (!MQTTclient.connected() && !MQTTConnectTimeout)  // loop till connected or timed out
      {
        Serial.println("MQTT not connected so Attempting MQTT connection...");
        // boolean connect(const char* id, const char* willTopic, uint8_t
        // willQos, boolean willRetain, const char* willMessage);
        if (MQTTclient.connect(MQTT_CLIENT_NAME, LWT_TOPIC, 1, true,
                               "Offline")) {
          // myWebSerial.println("connected to MQTT server");
          MQTTclient.publish(LWT_TOPIC, "Online", true);  // ensure send online
          // MQTTclient.publish(publishLWTTopic, "Online");
          MQTTclient.subscribe(subscribeTopic);
          // MQTTclient.subscribe(subscribeTopic2);
        //   MQTTclient.subscribe(subscribeTopic3);
        } else {
          // myWebSerial.println("MQTT connection failed, rc=");
          Serial.println(MQTTclient.state());
          // myWebSerial.println("MQTT STATE : ", MQTTclient.state());
          // myWebSerial.println(" try again ..");
        }
        now = millis();
        MQTTConnectTimeout = ((now - nowMillis) > timeOutMillis) ? true : false;
      }
      (!MQTTConnectTimeout)
          ? Serial.println("MQTT Connection made or already active !")
          : Serial.println("MQTT Connection attempt Timed Out!");
    // } else {  // we are already connected
    //   Serial.println("MQTT still connected -skip reconnect");
    // }
    now = millis();
    lastReconnectAttemptMillis = now;
  }
}


extern char *getTimeStr();

// MQTTclient call back if mqtt messsage rxed (cos has been subscribed  to)
void MQTTRxcallback(char *topic, byte *payload, unsigned int length) {
  uint8_t socketNumber = 0;

  // Power<x> 		Show current power state of relay<x> as On or
  // Off Power<x> 	0 / off 	Turn relay<x> power Off Power<x>
  // 1 / on 	Turn relay<x> power On handle message arrived mqtt
  //! TODO do some extra checking on rxed topic and payload?
  // payload[length] = '\0';

  // format and display the whole MQTT message and payload
  char fullMQTTmessage[255];  // = "MQTT rxed thisisthetopicforthismesage and
                              // finally the payload, and a bit extra to make
                              // sure there is room in the string and even more
                              // chars";
  strcpy(fullMQTTmessage, "MQTT Rxed [");
  strcat(fullMQTTmessage, topic);
  strcat(fullMQTTmessage, "]:");
  // append payload and add \o terminator
  strcat(fullMQTTmessage, "[");
  strncat(fullMQTTmessage, (char *)payload, length);
  strcat(fullMQTTmessage, "]");

  // Serial.println(fullMQTTmessage);
  Serial.println(fullMQTTmessage);
#ifdef DEBUG_WSERIAL

  myWebSerial.println(fullMQTTmessage);
  // Serial.print("1..");
#endif
  //! now store the topic and payload VIA REST POST to remote site DB
  // get the time mesage published - use now!//then add 3dp precision by
  // interrogating millis() for thousands of a sec (modulo????)

  // String published_at = timeClient.getFormattedDateTime();
  // TODO remote storage proven - remove it now
  // storeREST(topic, (char *)payload, (char *)published_at.c_str());

  // look for and process MQTT strings - if subscribed to, to act as additional
  // heartbeat from zones
  if (strstr(topic, "Zone1/HeartBeat") != NULL) {
    // ZCs[0].resetZoneDevice();
    // myWebSerial.print(getTimeStr());
    // myWebSerial.println("+> GGG MQTT HeartBeat Rxed");
    // strcpy(messageText, ZCs[0].heartBeatText);
  }
  if (strstr(topic, "Zone3/HeartBeat") != NULL) {
    // ZCs[2].resetZoneDevice();
    // myWebSerial.print(getTimeStr());
    // myWebSerial.println("+> SSS MQTT HeartBeat Rxed");
    // strcpy(messageText, ZCs[0].heartBeatText);
  }

  // only proces if topic starts with "433Bridge/cmnd/Power"
  if (strstr(topic, "433Bridge/cmnd/Power") != NULL) {
    // e.g incoming topic = "433Bridge/cmnd/Power1" to "...Power16", and payload
    // = 1 or 0 either match whole topic string or trim off last 1or 2 chars and
    // convert to a number, convert last 1-2 chars to socket number
    char lastChar =
        topic[strlen(topic) - 1];  // lst char will always be a digit char
    char lastButOneChar =
        topic[strlen(topic) - 2];  // see if last but 1 is also a digit char -
                                   // ie number has two digits - 10 to 16

    socketNumber = lastChar - '0';       // get actual numeric value
    if ((lastButOneChar == '1')) {       // it is a 2 digit number
      socketNumber = socketNumber + 10;  // calc actual int
    }

    // if ((payload[0] - '1') == 0) {
    //   newState = 1;
    // }
    Serial.print("......payload[");
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println("]");
    uint8_t newState = 0;  // default to off

    if ((char)(payload[0]) == '1') {
      newState = 1;
    }

    Serial.print(",,,,,,,,,,,,,,,,,,,,,,,,,,[");

    Serial.print(newState);
    Serial.println("]");

    // signal a new command has been rxed and
    // topic and payload also available
    MQTTNewState = newState;          // 0 or 1
    MQTTSocketNumber = socketNumber;  // 1-16
    MQTTNewData = true;
    return;
  }
  MQTTNewData = false;
}

// void MQTTLibSetup(void) {}
//#include "WebSerial.h"
// extern WebSerial myWebSerial;
// #include "My433Transmitter.h"
// extern My433Transmitter transmitter;
void processMQTTMessage(void) {
  // char msg[40] = "SSS == Operate Socket: ";
  char buff[10];

  // strcpy(buff, "Socket : ");
  // if socket number is  valid one -
  sprintf(buff, "%d", (MQTTSocketNumber));
  // strcat(msg, buff);
  // strcat(msg, "-");

  if (MQTTNewData) {
    // digitalWrite(ESP32_ONBOARD_BLUE_LED_PIN, MQTTNewState);
    // Serial
    // myWebSerial.println("process MQTT - MQTTSocketNumber...");

    // myWebSerial.println(buff);

    //                     myWebSerial.println("process MQTT -
    //                     MQTTNewState...");
    // sprintf(buff, "%d", (MQTTNewState));

    // myWebSerial.println(buff);

    // transmitter.operateSocket(MQTTSocketNumber - 1, MQTTNewState);
    MQTTNewData = false;  // indicate not new data now, processed
  }
}



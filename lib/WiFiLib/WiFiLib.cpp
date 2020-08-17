#include "WiFiLib.h"

#include "../../src/secret.h"

// WiFi settings
const char ssid[] = MY_SSID;
const char pass[] = MY_SSID_PASSWORD;
int status = WL_IDLE_STATUS;

void connectWiFi() {
  WiFi.setHostname("irbridge");
  WiFi.begin(ssid, pass);  // move out or if
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  //   printWifiStatus();
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

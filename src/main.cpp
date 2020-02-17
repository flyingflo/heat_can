/*
Based on Basic ESP8266 MQTT example and OTA Example
*/
#define NO_GLOBAL_MDNS
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ESP8266HTTPClient.h>

#include "common.h"

// Update these with values suitable for your network.

const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASSWD";
const char* mqtt_server = "mqtt.example.org";

const char* topic_status_conn = TOPIC_PREFIX  "status/connection";
const char* topic_sub = TOPIC_PREFIX "cmd/#";
const char* topic_ping = TOPIC_PREFIX "ping";
const char* topic_status_lockout = TOPIC_PREFIX "status/lockout";

static WiFiClient mqttSocket;
PubSubClient mqttClient(mqttSocket);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(128)
char msg[MSG_BUFFER_SIZE];
int ping_counter = 0;    

volatile bool lockout = false; // burner lockout
volatile bool _pub_lockout = false;
int _burner_on = -1;

const uint8_t pin_lockout = D4;
const uint8_t pin_burning = D2;

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.hostname(HOSTNAME);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("DHCP Hostname:");
  Serial.println(WiFi.hostname());
}

void publish_lockout() {
  mqttClient.publish(topic_status_lockout, lockout ? "1" : "0", true);
}

void callback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, TOPIC_PREFIX "cmd/lockout") == 0 && length > 0) {
    if ((char)payload[0] == '1') {
      digitalWrite(pin_lockout, LOW);
      lockout = true;
    } else {
      digitalWrite(pin_lockout, HIGH);
      lockout = false;
    }
    _pub_lockout =  true; 
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(HOSTNAME, topic_status_conn, false, true, "Offline")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttClient.publish(topic_status_conn, "Online, HELLO", true);
      // ... and resubscribe
      mqttClient.subscribe(topic_sub);
      _pub_lockout = true;
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup_ota() {
  Serial.println("Setup OTA");
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("heatpumpOTA");

  // No authentication by default
  ArduinoOTA.setPassword("Donn1EfBu");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin(false);
  Serial.println("OTA MDNS Hostname " + ArduinoOTA.getHostname());
}

void setup() {
  pinMode(pin_lockout, OUTPUT);
  digitalWrite(pin_lockout, HIGH); 
  lockout = false;
  pinMode(pin_burning, INPUT_PULLUP);

  Serial.begin(115200);
  setup_wifi();
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);
  setup_ota();
}

static unsigned long lastVZLog;

void logVZBurner(int burn) {
  WiFiClient cl;
  HTTPClient http;
  String url( burn ? 
    "http://pi3.lan/middleware/data/81b71d80-4ea8-11ea-a315-591170d780b2.json?value=100"
   : "http://pi3.lan/middleware/data/81b71d80-4ea8-11ea-a315-591170d780b2.json?value=0");
  http.begin(cl, url);
  int rc = http.POST("");
  if (rc != HTTP_CODE_OK) {
    mqttClient.publish(TOPIC_PREFIX "/status/vzError", http.errorToString(rc).c_str());
  }
  lastVZLog = millis();
}
void loop() {
  ArduinoOTA.handle();
  yield();

  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();

  yield();
  if (_pub_lockout) {
    publish_lockout();
    _pub_lockout = false;
  }

  int burner_on = !digitalRead(pin_burning);
  if (burner_on != _burner_on) {
    _burner_on = burner_on;
    mqttClient.publish(TOPIC_PREFIX "status/burnerOn", burner_on ? "1" : "0", true);
    logVZBurner(burner_on);
  }

  unsigned long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;
    ++ping_counter;
    snprintf (msg, MSG_BUFFER_SIZE, "#%d RSSI %d, BSSID %s", ping_counter, WiFi.RSSI(), WiFi.BSSIDstr().c_str());
    mqttClient.publish(topic_ping, msg);
  }

  yield();
  
  if (now - lastVZLog > 20*60*1000) {
    logVZBurner(_burner_on);
  }
}
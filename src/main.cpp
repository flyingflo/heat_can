/*
Based on Basic ESP8266 MQTT example and OTA Example
*/
#define NO_GLOBAL_MDNS
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

#include "common.h"
#include "logamaticCan.h"

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
    return;
  }
  LogamaticCAN.mqttRecv(topic, payload, length);
}

bool try_mqtt_reconnect() {
  static unsigned long lasttry = 0;
  // Loop until we're reconnected
  if (mqttClient.connected()) {
    return true;
  }
  unsigned long now = millis();
  if (now - lasttry < 200) {
    // wait a little longer ..
    return false;
  }
  lasttry = now;
  Serial.print("Attempting MQTT connection...");
  // Attempt to connect
  if (mqttClient.connect(HOSTNAME, topic_status_conn, false, true, "Offline")) {
    Serial.println("connected");
    // Once connected, publish an announcement...
    mqttClient.publish(topic_status_conn, "Online, HELLO", true);
    // ... and resubscribe
    mqttClient.subscribe(topic_sub);
    mqttClient.subscribe(LogamaticCAN.sub_topic());
    _pub_lockout = true;
    return true;
  } else {
    Serial.print("failed, rc=");
    Serial.print(mqttClient.state());
    Serial.println(" try again in 5 seconds");
    return false;
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
  LogamaticCAN.setup();
  setup_wifi();
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);
  setup_ota();
}

static unsigned long loopentry;
static void pingStats() {
  static unsigned long lastMsg = 0;
  const int MSG_BUFFER_SIZE = 128;
  static char msg[MSG_BUFFER_SIZE];
  static unsigned ping_counter = 0;    
  static unsigned long last = 0;
  static unsigned long max = 0;
  static unsigned count = 0; 
  static unsigned maxrun = 0;
  count++;
  unsigned long now = millis();
  if (now - last > max) {
    max = now - last;
    last = now;
  }

  if (now - loopentry > maxrun) {
    maxrun = now - loopentry;
  }
  unsigned long interval = now - lastMsg;
  if (interval > 10000) {
    int rate = count * 1000 / interval;
    ++ping_counter;
    snprintf (msg, MSG_BUFFER_SIZE, "RSSI %d, BSSID %s interval_loop max %lu avg %d/s runt_loop max %u #%u", WiFi.RSSI(), WiFi.BSSIDstr().c_str(), max, rate, maxrun, ping_counter);
    Serial.println(msg);
    mqttClient.publish(topic_ping, msg);
    lastMsg = now;
    count = 0;
    max = 0;
    maxrun = 0;
  }
}


void loop() {
  loopentry = millis();
  ArduinoOTA.handle();
  yield();

  bool mqtt_connected = try_mqtt_reconnect();
  if (mqtt_connected) {
    mqttClient.loop();

    yield();
    if (_pub_lockout) {
      publish_lockout();
      _pub_lockout = false;
    }

    yield();

    int burner_on = !digitalRead(pin_burning);
    if (burner_on != _burner_on) {
      _burner_on = burner_on;
      mqttClient.publish(TOPIC_PREFIX "status/burnerOn", burner_on ? "1" : "0", true);
    }

    yield();

    LogamaticCAN.loop();
  }
  pingStats();
}
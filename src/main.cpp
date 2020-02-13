/*
 Basic ESP8266 MQTT example
 This sketch demonstrates the capabilities of the pubsub library in combination
 with the ESP8266 board/library.
 It connects to an MQTT server then:
  - publishes "hello world" to the topic "outTopic" every two seconds
  - subscribes to the topic "inTopic", printing out any messages
    it receives. NB - it assumes the received payloads are strings not binary
  - If the first character of the topic "inTopic" is an 1, switch ON the ESP Led,
    else switch it off
 It will reconnect to the server if the connection is lost using a blocking
 reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
 achieve the same result without blocking the main loop.
 To install the ESP8266 board, (using Arduino 1.6.4+):
  - Add the following 3rd party board manager under "File -> Preferences -> Additional Boards Manager URLs":
       http://arduino.esp8266.com/stable/package_esp8266com_index.json
  - Open the "Tools -> Board -> Board Manager" and click install for the ESP8266"
  - Select your ESP8266 in "Tools -> Board"
*/
#define NO_GLOBAL_MDNS
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ESP8266HTTPClient.h>

// Update these with values suitable for your network.

const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASSWD";
const char* mqtt_server = "mqtt.example.org";

#define HOSTNAME "ESP-burner"
#define TOPIC_PREFIX "/heizung/burner/"
const char* topic_status_conn = TOPIC_PREFIX  "status/connection";
const char* topic_sub = TOPIC_PREFIX "cmd/#";
const char* topic_ping = TOPIC_PREFIX "ping";
const char* topic_status_lockout = TOPIC_PREFIX "status/lockout";

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(128)
char msg[MSG_BUFFER_SIZE];
int ping_counter = 0;    

bool lockout = false; // burner lockout
bool _pub_lockout = false;
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
  client.publish(topic_status_lockout, lockout ? "1" : "0", true);
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
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(HOSTNAME, topic_status_conn, false, true, "Offline")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(topic_status_conn, "Online, HELLO", true);
      // ... and resubscribe
      client.subscribe(topic_sub);
      _pub_lockout = true;
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
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
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  setup_ota();
}

void logVZBurner(int burn) {
  WiFiClient cl;
  HTTPClient http;
  String url( burn ? 
    "http://pi3.lan/middleware/data/81b71d80-4ea8-11ea-a315-591170d780b2.json?value=100"
   : "http://pi3.lan/middleware/data/81b71d80-4ea8-11ea-a315-591170d780b2.json?value=0");
  http.begin(cl, url);
  int rc = http.POST("");
  if (rc != HTTP_CODE_OK) {
    client.publish(TOPIC_PREFIX "/status/vzError", http.errorToString(rc).c_str());
  }
}
void loop() {
  ArduinoOTA.handle();
  yield();

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  yield();
  if (_pub_lockout) {
    publish_lockout();
    _pub_lockout = false;
  }

  int burner_on = !digitalRead(pin_burning);
  if (burner_on != _burner_on) {
    _burner_on = burner_on;
    client.publish(TOPIC_PREFIX "status/burnerOn", burner_on ? "1" : "0", true);
    logVZBurner(burner_on);
  }

  unsigned long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;
    ++ping_counter;
    snprintf (msg, MSG_BUFFER_SIZE, "#%d RSSI %d, BSSID %s", ping_counter, WiFi.RSSI(), WiFi.BSSIDstr().c_str());
    client.publish(topic_ping, msg);
  }
}
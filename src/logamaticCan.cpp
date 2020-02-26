#include <CAN.h>
#include "logamaticCan.h"
#include "common.h"

static const long bitrates[] = {
    (long)1000E3,
    (long)500E3,
    (long)250E3,
    (long)200E3,
    (long)125E3,
    (long)100E3,
     (long)80E3,
     (long)50E3,
     (long)40E3,
     (long)20E3,
     (long)10E3,
      (long)5E3, 
 };

static char buf[42];

void logamaticCan::setup() {
    // SPI.pins(); //use the default HSPI pins
    // SPI.setHwCs(true);  // seems to collide with CAN library's CS
    CAN.setPins(D8, D1);
    CAN.setClockFrequency(8E6);
    int rc = CAN.begin(bitrates[_bitrateIdx]);
    Serial.print("CAN.begin(");
    Serial.print(bitrates[_bitrateIdx]);
    Serial.print(")-> ");
    Serial.println(rc);
    if (rc == 1) {
        // CAN.onReceive(onReceive);
    }
}

void logamaticCan::mqttRecv(char* topic, byte* payload, unsigned int length) {
    if (strcmp(send_topic, topic) == 0 && length > 0) {
        handleSend(payload, length);
    }
}

void logamaticCan::handleSend(byte* payload, unsigned int length) {
    int rtr;
    long id;
    uint8_t data[8];
    unsigned l = sizeof(buf) -1 < length ? sizeof(buf) : length;
    memcpy(buf, payload, l);
    buf[l] = '\0';
    int r = sscanf(buf, "%x;%lx; %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx", 
            &rtr, &id, &data[0], &data[1], &data[2], &data[3], 
            &data[4], &data[5], &data[6], &data[7]);
    if (r != 10) {
        const char errmsg[] = "MQTT payload";
        mqttClient.beginPublish(TOPIC_PREFIX "can/err", sizeof(errmsg), false);
        mqttClient.write((uint8_t*)errmsg, sizeof(errmsg));
        mqttClient.endPublish();
        return;
    }
    bool ok = CAN.beginPacket(id, rtr) &&
    CAN.write(data, 8) == 8 &&
    CAN.endPacket();

    if (!ok) {
        const char errmsg[] = "CAN write";
        mqttClient.beginPublish(TOPIC_PREFIX "can/err", sizeof(errmsg), false);
        mqttClient.write((uint8_t*)errmsg, sizeof(errmsg));
        mqttClient.endPublish();
    } else {
        const char msg[] = "CAN write success";
        mqttClient.beginPublish(TOPIC_PREFIX "can/dbg", sizeof(msg), false);
        mqttClient.write((uint8_t*)msg, sizeof(msg));
        mqttClient.endPublish();

    }
}

void logamaticCan::handleRecv() {
    int d;
    int sz;
    while((sz = CAN.parsePacket()) > 0) {
        _lastrecv = millis();
        int bi = snprintf(buf, sizeof(buf), "%x;%lx;", CAN.packetRtr(), CAN.packetId());
        while ((d = CAN.read()) >= 0) {
            bi += snprintf(buf + bi, sizeof(buf) - bi, "%02x ", d);
        }
        if (mqttClient.beginPublish(TOPIC_PREFIX "can/raw/recv/", bi, false)) {
            mqttClient.write((uint8_t*)buf, bi);
            mqttClient.endPublish();
        }
    }
}

void logamaticCan::loop() {
    handleRecv();

}

logamaticCan LogamaticCAN;
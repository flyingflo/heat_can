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

struct CanPacket {
    long id;
    bool rtr;
    bool fresh;
    int overruns;
    uint8_t dlc;
    uint8_t data[8];
    uint8_t datalen;
};

static volatile CanPacket _packet;

static void receive(int size) {
    if (_packet.fresh) {
        _packet.overruns++;
        return;
    }
    _packet.id = CAN.packetId();
    _packet.rtr = CAN.packetRtr();
    _packet.dlc = CAN.packetDlc();
    int i;
    for (i = 0; i < size; i++) {
        int b = CAN.read();
        if (b < 0) {
            break;
        }
        _packet.data[i] = (uint8_t)b;
    }
    _packet.datalen = i;
    _packet.fresh = true;
}

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
    if (strcmp(TOPIC_PREFIX "cmd/can/ctrl/setBitrate", topic) == 0 && length > 0) {
        char buf[3];
        memcpy(buf, payload, length > 2 ? 2 : length);
        buf[2] = '\0';
        int p = atoi(buf);
        if (p >=0 && p < (sizeof(bitrates) / sizeof(bitrates[0]))) {
            _bitrateIdx = p;
            _resetBitrate = true;
        }
    }
}

void logamaticCan::handleRecv() {
    static char buf[42];
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
void logamaticCan::handleBitrate() {
    if (_resetBitrate) {
        CAN.end();
        int rc = CAN.begin(bitrates[_bitrateIdx]);
        Serial.print("CAN.begin(");
        Serial.print(bitrates[_bitrateIdx]);
        Serial.print(")-> ");
        Serial.println(rc);
        
        char msg[16];
        unsigned l = snprintf(msg, sizeof(msg), "%s %ld", rc == 1 ? "OK" : "NG", bitrates[_bitrateIdx]);
        mqttClient.publish(TOPIC_PREFIX "can/status/resetBitrate", msg, l);
        _resetBitrate = false;
    }
}

static void loopbackTest() {
    static bool once = false;

    if (!once && millis() > 10000) {
        int rc = CAN.loopback();
        Serial.print("loopback");
        Serial.println(rc);
        CAN.beginPacket(0xbc);
        CAN.write(0x42);
        rc = CAN.endPacket();
        Serial.print("send");
        Serial.println(rc);
        once = true;
    }
}

void logamaticCan::loop() {
    handleRecv();
    handleBitrate();

}

logamaticCan LogamaticCAN;
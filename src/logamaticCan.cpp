#include <CAN.h>
#include "logamaticCan.h"

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

static void onReceive(int size) {
    if (_packet.fresh) {
        _packet.overruns++;
        return;
    }
    _packet.id = CAN.packetId();
    _packet.rtr = CAN.packetRtr();
    _packet.dlc = CAN.packetDlc();
    unsigned i;
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
    // SPI.pins(); use the default HSPI pins
    SPI.setHwCs(true);
    CAN.setClockFrequency(8E6);
    CAN.begin(125E3);
    CAN.onReceive(onReceive);
}

void logamaticCan::loop() {


}
#include "common.h"

class logamaticCan {

    unsigned long _lastrecv;
    volatile long _bitrateIdx = 7;

    void handleRecv();
    void handleSend(byte* payload, unsigned int length);
public:
    void setup();
    void loop();
    void mqttRecv(char* topic, byte* payload, unsigned int length);
    const char*  send_topic = TOPIC_PREFIX "can/raw/send";

};

extern logamaticCan LogamaticCAN;
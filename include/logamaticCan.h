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
    const char* sub_topic() {
        const char* t = TOPIC_PREFIX "can/send";
        return t;
    }

};

extern logamaticCan LogamaticCAN;
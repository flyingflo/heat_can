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

    int _rec = -1;   //< Receive error counter
    int _tec = -1;   //< Transmit error counter
    int _eflg = -1;  //< Error flags
    unsigned int _recv_overflows = 0;
    
    void checkErrors();

};

extern logamaticCan LogamaticCAN;
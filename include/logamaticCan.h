
class logamaticCan {

    unsigned long _lastrecv;
    volatile long _bitrateIdx = 7;
    volatile long _resetBitrate = false;

    void handleBitrate();
    void handleRecv();
    void handleSend(byte* payload, unsigned int length);
public:
    void setup();
    void loop();
    void mqttRecv(char* topic, byte* payload, unsigned int length);

};

extern logamaticCan LogamaticCAN;
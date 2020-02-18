#ifndef COMMON
#define COMMON

#define HOSTNAME "ESP-burner"
#define TOPIC_PREFIX "/heizung/burner/"

#include <PubSubClient.h>
extern PubSubClient mqttClient;
#endif /* COMMON */

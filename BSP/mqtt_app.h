#ifndef __MQTT_APP_H__
#define __MQTT_APP_H__

#include "MQTTClient.h"
#include "mqtt_interface.h"

#define MQTT_SOCKET 2
#define MQTT_BUF_SIZE 2048
#define MQTT_BROKER_IP {192,168,227,10}
#define MQTT_BROKER_PORT 1883
#define CLIENT_ID "STM32_W5500"
#define PUB_TOPIC "device/data"
#define SUB_TOPIC "device/control"
void mqtt_init(void);
void mqtt_loop(void);
void Publish_Test_Data(void);
extern  MQTTClient g_MQTTclient;
#endif /* __MQTT_APP_H__ */

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

typedef enum {
    DEVICE_LAMP = 0,
    DEVICE_FAN,
    DEVICE_SPRAY,
    DEVICE_ALARM,
    DEVICE_MAX
} DeviceType_t;

// ?ùù????????
static const char* g_device_names[DEVICE_MAX] = {
    "lamp",
    "fun",
    "spray",
    "alarm"
};
#define DEVICE_LAMP_ID 0
#define DEVICE_FUN_ID 1
#define DEVICE_SPRAY_ID 2

void MQTT_Publish_Alert(uint16_t co2_value,uint8_t alarm);
void Publish_sensor_temp_humi(void);
void Send_All_Device_Status(void);
void Send_Lamp_Command(uint8_t state, uint8_t id);
void Send_fun_Command(uint8_t state, uint8_t id);
void Send_spray_Command(uint8_t state, uint8_t id);
#endif /* __MQTT_APP_H__ */

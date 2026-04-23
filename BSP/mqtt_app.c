#include "mqtt_app.h"
#include "MQTTClient.h"
#include "cJSON.h"
#include "control.h"
#include "main.h"
#include "mqtt_interface.h"
#include "sensor.h"
#include "socket.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern Control_Device_t g_device;
static uint8_t mqtt_rx_buf[MQTT_BUF_SIZE];
static uint8_t mqtt_tx_buf[MQTT_BUF_SIZE];
static uint32_t publish_counter = 0;
static Network g_network;
MQTTClient g_MQTTclient;
static char payload_mqtt_buff[512];

void Publish_Test_Data(void) {
  char json_msg[128];
  sprintf(json_msg, "{\"type\":\"heartbeat\",\"counter\":%d,\"timestamp\":%d}",
          publish_counter++, (int)HAL_GetTick());
  MQTTMessage pub_msg;
  pub_msg.qos = QOS0;
  pub_msg.retained = 0;
  pub_msg.payload = json_msg;
  pub_msg.payloadlen = strlen(json_msg);
  MQTTPublish(&g_MQTTclient, PUB_TOPIC, &pub_msg);
}
void Publish_Device_Command(const char *device_name, uint8_t state,
                            uint8_t device_id) {
  cJSON *root = cJSON_CreateObject();
  if (root == NULL) {
    printf("Create failed \r\n");
    return;
  }
  cJSON_AddBoolToObject(root, device_name, state ? true : false);
  cJSON_AddNumberToObject(root, "id", device_id);
  if (!cJSON_PrintPreallocated(root, payload_mqtt_buff,
                               sizeof(payload_mqtt_buff), 0)) {
    printf("JSON string create failed\r\n");
    cJSON_Delete(root);
    return;
  }
  MQTTMessage msg;
  msg.qos = QOS0;
  msg.payload = payload_mqtt_buff;
  msg.payloadlen = strlen(payload_mqtt_buff);
  msg.retained = 0;
  int rc = MQTTPublish(&g_MQTTclient, PUB_TOPIC, &msg);
  if (rc != SUCCESSS) {
    printf("[ERROR] MQTT publish failed, rc=%d\r\n", rc);
  }
  cJSON_Delete(root);
}
void Send_Lamp_Command(uint8_t state, uint8_t id) {
  Publish_Device_Command("lamp", state, id);
}
void Send_fun_Command(uint8_t state, uint8_t id) {
  Publish_Device_Command("fun", state, id);
}
void Send_spray_Command(uint8_t state, uint8_t id) {
  Publish_Device_Command("spray", state, id);
}

// ???????????��??
void Send_All_Device_Status(void) {
  cJSON *root = cJSON_CreateObject();
  if (root == NULL) {
    return;
  }

  // ?????��??
  cJSON_AddBoolToObject(root, "lamp", g_device.lamp_state ? true : false);
  cJSON_AddBoolToObject(root, "fun", g_device.fun_state ? true : false);
  cJSON_AddBoolToObject(root, "spray", g_device.spray_state ? true : false);

  // ?????????????
  float temp = 0.0f;
  float humi = 0.0f;
  Sensor_Read_TempHumi(&temp, &humi);
  cJSON_AddNumberToObject(root, "temp", temp);
  cJSON_AddNumberToObject(root, "humi", humi);
  uint16_t co2 = Sensor_Read_CO2();
  cJSON_AddNumberToObject(root, "co2", co2);
  cJSON_AddBoolToObject(root, "co2_alarm", g_device.co2_alarm ? true : false);

  // ????????
  cJSON_AddNumberToObject(root, "timestamp", HAL_GetTick());

  // ?????��???
  cJSON_AddStringToObject(root, "device_id", "STM32_W5500");
  cJSON_AddNumberToObject(root, "version", 1);
  if (!cJSON_PrintPreallocated(root, payload_mqtt_buff,
                               sizeof(payload_mqtt_buff), 0)) {
    if (g_MQTTclient.isconnected) {
      printf("[ERROR] Send_All_Device_Status: JSON string creation failed\r\n");
    }
    cJSON_Delete(root);
    return;
  }

  MQTTMessage msg;
  msg.qos = QOS0;
  msg.retained = 0;
  msg.payload = (void *)payload_mqtt_buff;
  msg.payloadlen = strlen(payload_mqtt_buff);

  int rc = MQTTPublish(&g_MQTTclient, PUB_TOPIC, &msg);
  if (rc != SUCCESSS && g_MQTTclient.isconnected) {
    printf("[ERROR] MQTT publish failed, rc=%d\r\n", rc);
  }

  cJSON_Delete(root);
}
void Publish_sensor_temp_humi(void) {
  if (!g_MQTTclient.isconnected)
    return;

  float temp, humi;
  Sensor_Read_TempHumi(&temp, &humi);
  cJSON *root = cJSON_CreateObject();
  if (root == NULL) {
    return;
  }
  cJSON_AddNumberToObject(root, "temp", temp);
  cJSON_AddNumberToObject(root, "humi", humi);
  if (!cJSON_PrintPreallocated(root, payload_mqtt_buff,
                               sizeof(payload_mqtt_buff), 0)) {
    cJSON_Delete(root);
    return;
  }
  MQTTMessage msg;
  msg.qos = QOS0;
  msg.payload = payload_mqtt_buff;
  msg.payloadlen = strlen(payload_mqtt_buff);
  msg.retained = 0;
  int rc = MQTTPublish(&g_MQTTclient, PUB_TOPIC, &msg);
  if (rc != SUCCESSS && g_MQTTclient.isconnected) {
    printf("[ERROR] MQTT publish failed, rc=%d\r\n", rc);
  }
  cJSON_Delete(root);
}
void MQTT_Publish_Alert(uint16_t co2_value, uint8_t alarm) {
  if (!g_MQTTclient.isconnected)
    return;

  cJSON *root = cJSON_CreateObject();
  if (root == NULL) {
    return;
  }

  cJSON_AddNumberToObject(root, "co2", co2_value);
  cJSON_AddBoolToObject(root, "alert", alarm ? true : false);
  cJSON_AddStringToObject(root, "status", alarm ? "alert" : "normal");
  cJSON_AddNumberToObject(root, "timestamp", HAL_GetTick());
  if (!cJSON_PrintPreallocated(root, payload_mqtt_buff,
                               sizeof(payload_mqtt_buff), 0)) {
    cJSON_Delete(root);
    return;
  }

  MQTTMessage msg;
  msg.qos = QOS0;
  msg.retained = 0;
  msg.payload = payload_mqtt_buff;
  msg.payloadlen = strlen(payload_mqtt_buff);

  int rc = MQTTPublish(&g_MQTTclient, PUB_TOPIC, &msg);

  if (rc != SUCCESSS && g_MQTTclient.isconnected) {
    printf("[ERROR] MQTT publish failed, rc=%d\r\n", rc);
  }
  cJSON_Delete(root);
}
//{"lamp":false,"id":0}
static uint8_t Parse_JSON_Command(char *payload) {
  cJSON *root = NULL;

  // 1. �������ܵ����������ʽ: [{"fun":false,"id":3}]
  // ����յ��������飬�ȳ��Խ������鲢ȡ��һ������
  root = cJSON_Parse(payload);
  if (root == NULL) {
    printf("JSON Parse failed!\r\n");
    return 0;
  }

  cJSON *item = root;
  if (cJSON_IsArray(root)) {
    item = cJSON_GetArrayItem(root, 0);
    if (item == NULL) {
      cJSON_Delete(root);
      return 0;
    }
  }

  printf("JSON Parse SUCCESS!\r\n");

  // 2. �����豸����
  int parsed = 0;
  for (int i = 0; i < DEVICE_MAX; i++) {
    cJSON *device_item = cJSON_GetObjectItem(item, g_device_names[i]);
    if (device_item != NULL) {
      if (cJSON_IsBool(device_item)) {
        uint8_t new_state = (device_item->type == cJSON_True) ? 1 : 0;
        parsed = 1;
        device_item = cJSON_GetObjectItem(item, "id");
        if (device_item == NULL) {
          cJSON_Delete(root);
          return 0;
        }
        int id = device_item->valuedouble;
        switch (i) {
        case DEVICE_LAMP:
          if (id == DEVICE_LAMP_ID) {
            g_device.lamp_state = new_state;
            Control_lamp(g_device.lamp_state);
          }
          break;
        case DEVICE_FAN:
          if (id == DEVICE_FUN_ID) {
            g_device.fun_state = new_state;
            Control_fun(g_device.fun_state);
          }
          break;
        case DEVICE_SPRAY:
          if (id == DEVICE_SPRAY_ID) {
            g_device.spray_state = new_state;
            Control_spray(g_device.spray_state);
          }
          break;
        }
      }
    }
  }

  // 3. ���� ID
  cJSON *id_item = cJSON_GetObjectItem(item, "id");
  if (id_item != NULL && cJSON_IsNumber(id_item)) {
    printf("Message ID: %d\r\n", (int)id_item->valuedouble);
  }

  cJSON_Delete(root);
  return parsed;
}

static void messageArrived(MessageData *md) {
  MQTTMessage *message = md->message;

  // ????????
  char payload[256];
  memset(payload, 0, sizeof(payload));
  int copy_len = (message->payloadlen < sizeof(payload) - 1)
                     ? message->payloadlen
                     : sizeof(payload) - 1;
  memcpy(payload, message->payload, copy_len);

  printf("Copied payload: [%s]\r\n", payload);
  printf("Copied length: %d\r\n", (int)strlen(payload));

  if (Parse_JSON_Command(payload)) {
    printf("[INFO] JSON command processed\r\n");
    return;
  }

  // Echo ????
  printf("Echoing message...\r\n");
  MQTTMessage pub_msg;
  pub_msg.qos = QOS0;
  pub_msg.retained = 0;
  pub_msg.payload = message->payload;
  pub_msg.payloadlen = message->payloadlen;
  MQTTPublish(&g_MQTTclient, PUB_TOPIC, &pub_msg);
  printf("Echo sent to topic: %s\r\n", PUB_TOPIC);
}

void mqtt_init(void) {
  uint8_t broker_ip[4] = MQTT_BROKER_IP;
  printf("Connecting to Broker %d.%d.%d.%d...\r\n", broker_ip[0], broker_ip[1],
         broker_ip[2], broker_ip[3]);
  // 1. ??????????
  NewNetwork(&g_network, MQTT_SOCKET);
  // 2. TCP ????????
  if (ConnectNetwork(&g_network, broker_ip, MQTT_BROKER_PORT) != SOCK_OK) {
    printf("TCP Connection Failed!\r\n");
    return;
  }
  printf("TCP Connected.\r\n");

  // �� TCP ����һ���ȶ�ʱ��
  HAL_Delay(100);

  // 3. ????? MQTT ????????
  // �����ʱʱ��� 1000ms ���ӵ� 5000ms����
  // Broker ����Ĵ���ʱ��
  MQTTClientInit(&g_MQTTclient, &g_network, 5000, mqtt_tx_buf, MQTT_BUF_SIZE,
                 mqtt_rx_buf, MQTT_BUF_SIZE);
  // 4. MQTT ????????? (Connect)
  MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
  data.willFlag = 0;
  data.MQTTVersion = 3; // MQTT v3.1.1
  data.clientID.cstring = CLIENT_ID;
  data.keepAliveInterval = 60;
  data.cleansession = 1;

  int rc = MQTTConnect(&g_MQTTclient, &data);
  if (rc != SUCCESSS) {
    printf("MQTT Connect Failed! Return Code: %d\r\n", rc);
    return;
  }
  printf("MQTT Protocol Connected!\r\n");

  // 5. ���Ŀ�������
  rc = MQTTSubscribe(&g_MQTTclient, SUB_TOPIC, QOS0, messageArrived);
  if (rc != SUCCESSS) {
    printf("Subscribe Failed! Return Code: %d\r\n", rc);
  } else {
    printf("Subscribed to topic: %s\r\n", SUB_TOPIC);
  }

  // 6. �Ƴ�ǿ���л�������ģʽ���߼�
  // ���ڵ� w5x00_read �ڲ���ͨ����� RSR
  // �Ĵ���ʵ���߼��ϵķ����� ���ͨ��
  // ctlsocket �л������ȶ�
  printf("MQTT Protocol Ready (Software Non-blocking).\r\n");
}

void mqtt_loop(void) {
  static uint32_t last_reconnect_tick = 0;
  uint32_t now = HAL_GetTick();

  if (g_MQTTclient.isconnected) {
    // 1. ���� MQTT ���� (�߼�������)
    MQTTYield(&g_MQTTclient, 50);

    // ���Ӳ���Ƿ��� (��ֹ SPI ����)
    if (getVERSIONR() != 0x04) {
      printf("[CRITICAL] W5500 hardware lost! Resetting connection...\r\n");
      g_MQTTclient.isconnected = 0;
    }
  } else {
    // 2. �����Զ������߼� (ÿ 5 �볢��һ��)
    if (now - last_reconnect_tick > 5000) {
      last_reconnect_tick = now;
      printf("[RECONNECT] Attempting to reconnect MQTT...\r\n");
      mqtt_init();
    }
  }
}

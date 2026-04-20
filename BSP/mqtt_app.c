#include "mqtt_app.h"
#include "MQTTClient.h"
#include "mqtt_interface.h"
#include "socket.h"
#include <stdio.h>
#include <string.h>
#include "main.h"
#include <stdlib.h>  
#include "cJSON.h"
static uint8_t mqtt_rx_buf[MQTT_BUF_SIZE];
static uint8_t mqtt_tx_buf[MQTT_BUF_SIZE];
static uint32_t publish_counter = 0;
static Network g_network;
 MQTTClient g_MQTTclient;

static uint8_t led1_status = 0;
static uint8_t led2_status = 0;


void Publish_Test_Data(void)
{
char json_msg[128];
	sprintf(json_msg,"{\"type\":\"heartbeat\",\"counter\":%d,\"timestamp\":%d}",publish_counter++,(int)HAL_GetTick());
	MQTTMessage pub_msg;
	pub_msg.qos = QOS0;
	pub_msg.retained = 0;
	pub_msg.payload = (void *)json_msg;
	pub_msg.payloadlen = strlen(json_msg);
	MQTTPublish(&g_MQTTclient,PUB_TOPIC,&pub_msg);
}
/**
 * @brief 消息到达回调函数 (Echo 功能)
 */
 
void  Send_LED_Status_JSON(uint8_t led_id,uint8_t status)
{
	char json_msg[128];
	sprintf(json_msg,"{\"led_id\":%d,\"status:\":%s,\"timestamp\":%d}",led_id,status?"true":"false",(int)HAL_GetTick());
	MQTTMessage pub_msg;
	pub_msg.qos = QOS0;
	pub_msg.retained = 0;
	pub_msg.payload = (void *)json_msg;
	pub_msg.payloadlen = strlen(json_msg);
	if(MQTTPublish(&g_MQTTclient,PUB_TOPIC,&pub_msg) ==SUCCESSS)
	{
		printf("[JSON RESPONSE] %s\r\n", json_msg);
	}
	else {
        printf("[ERROR] Failed to send JSON response\r\n");
    }
}
void Control_LED(uint8_t led_id,char* action)
{
	switch(led_id)
	{
		case 1:
			if(strcmp(action,"on") == 0)
			{
			led1_status = 1;
      printf("[LED1] Turned ON\r\n");
			}
			else
			{
			led1_status = 0;
      printf("[LED1] Turned OFF\r\n");
			}
			break;
		case 2:
			if(strcmp(action,"on") == 0)
			{
			led2_status = 1;
      printf("[LED2] Turned ON\r\n");
			}
			else
			{
			led2_status = 0;
      printf("[LED2] Turned OFF\r\n");
			}
			break;
	}

}

static uint8_t Parse_JSON_Command(char *payload)
{
	cJSON *root = NULL;
	int led_id = 0;
	char* action = {0};
	
	root = cJSON_Parse(payload);//判断是不是JSON格式
	if(!root)
	{
	printf("JSON Parse faild\r\n");
	return 0;
	}
	cJSON *led_id_item =cJSON_GetObjectItemCaseSensitive(root,"led_id");//查找led_id的
	if(led_id_item == NULL || !cJSON_IsNumber(led_id_item))
	{
		cJSON_Delete(root);
		return 0;
	}
	cJSON *action_item = cJSON_GetObjectItemCaseSensitive(root,"action");
	
	if(action_item == NULL || !cJSON_IsString(action_item))
	{
		cJSON_Delete(root);
		return 0;
	}
	led_id = led_id_item->valuedouble;
	action = action_item->valuestring;
	printf("LED %d action: %s\n", led_id, action);
	Control_LED(led_id,action);
	cJSON_Delete(root);
	return 1;
}

static void messageArrived(MessageData *md) {
  MQTTMessage *message = md->message;
  MQTTString *topic = md->topicName;
	char *payload = (char *)message->payload;
	
  // 1. 打印收到的消息
  printf("Topic: %.*s, Payload: %.*s\r\n", topic->lenstring.len,
         topic->lenstring.data, (int)message->payloadlen,
         (char *)message->payload);

				 if(Parse_JSON_Command(payload))
				 {
					printf("[INFO] JSON command processed\r\n");
					return;
				 }
  // 2. LED 控制示例
  if (strncmp((char *)message->payload, "LED_ON", message->payloadlen) == 0) {
    printf("Action: LED ON\r\n");
  } 
	else if (strncmp((char *)message->payload, "LED_OFF",
                     message->payloadlen) == 0) {
    printf("Action: LED OFF\r\n");
  }

  // 3. 数据回传 (Echo)
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
  // 1. 初始化网络层
  NewNetwork(&g_network, MQTT_SOCKET);
  // 2. TCP 三次握手
  if (ConnectNetwork(&g_network, broker_ip, MQTT_BROKER_PORT) != SOCK_OK) {
    printf("TCP Connection Failed!\r\n");
    return;
  }
  printf("TCP Connected.\r\n");
  // 3. 初始化 MQTT 客户端对象
  MQTTClientInit(&g_MQTTclient, &g_network, 1000, mqtt_tx_buf, MQTT_BUF_SIZE,
                 mqtt_rx_buf, MQTT_BUF_SIZE);
  // 4. MQTT 协议层连接 (Connect)
  MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
  data.willFlag = 0;
  data.MQTTVersion = 3;
  data.clientID.cstring = CLIENT_ID;
  data.keepAliveInterval = 60;
  data.cleansession = 1;
  if (MQTTConnect(&g_MQTTclient, &data) != SUCCESSS) {
    printf("MQTT Connect Failed!\r\n");
    return;
  }
  printf("MQTT Protocol Connected!\r\n");
  // 5. 订阅控制主题
  if (MQTTSubscribe(&g_MQTTclient, SUB_TOPIC, QOS0, messageArrived) !=
      SUCCESSS) {
    printf("Subscribe Failed!\r\n");
  } else {
    printf("Subscribed to topic: %s\r\n", SUB_TOPIC);
  }
}


void mqtt_loop(void) {
  MQTTYield(&g_MQTTclient, 100);
}

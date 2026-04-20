#ifndef __ESP8266_H
#define __ESP8266_H

#include "string.h"
#include "stdio.h"
#include "usart.h"
      
#define REV_OK 0
#define REV_WAIT 1
#define ESP8266_WIFI_INFO  "AT+CWJAP=\"yiyi\",\"1004991014\"\r\n"
#define ESP8266_ONENET_INFO  		"AT+MQTTCONN=0,\"mqtts.heclouds.com\",1883,1\r\n"
#define ESP8266_USERCFG_INFO    "AT+MQTTUSERCFG=0,1,\"DHT11\",\"BPiBi8XrF9\",\"version=2018-10-31&res=products%2FBPiBi8XrF9%2Fdevices%2FDHT11&et=2810313550&method=md5&sign=sag6Ii3Ip7vRxgR3Me4mew%3D%3D\",0,0,\"\"\r\n"
#define ESP8266_DHTP  "AT+CWDHCP=1,1\r\n"

extern unsigned char esp8266_buf[400];
extern unsigned short esp8266_cnt,esp8266_cntPre;
extern const  char* pubtopic;

void ESP8266_Init(void);
void ESP8266_Clear(void);
_Bool ESP8266_SendCmd(char *cmd, char *res);
uint8_t ESP8266_WaitRecive(void);
void SendData(float temp,float humi);
#endif

#include "ESP8266.h"
#include "tim.h"
const  char* pubtopic="$sys/BPiBi8XrF9/DHT11/thing/property/post";
	unsigned char esp8266_buf[400];
unsigned short esp8266_cnt = 0, esp8266_cntPre = 0;

void ESP8266_Clear(void)
{
memset(esp8266_buf,0,sizeof(esp8266_buf));
esp8266_cnt=0;
}

uint8_t ESP8266_WaitRecive(void)
{
	if(esp8266_cnt==0)
		return REV_WAIT;
if(esp8266_cnt==esp8266_cntPre)
{
esp8266_cnt=0;
	return REV_OK;
}
esp8266_cntPre=esp8266_cnt;
return REV_WAIT;
}
	


_Bool ESP8266_SendCmd(char * cmd,char*res)	
{

	unsigned char timeout = 250;

	HAL_UART_Transmit(&huart2,(uint8_t *)cmd,strlen(cmd),30000);
while(timeout--)
	{
		
		if(ESP8266_WaitRecive() == REV_OK)							//如果收到数据
		{
			printf("data:%s\r\n",esp8266_buf);
			if(strstr((const char *)esp8266_buf, res) != NULL)		//如果检索到关键词
			{	
				ESP8266_Clear();
				return 0;
			}
				
		}
	HAL_Delay(10);
	}
	return 1;
}
	


#define ESP_MODE "AT+CWMODE=1\r\n"
void ESP8266_Init(void)
{
    ESP8266_Clear();

    printf("[ESP] 1. 重启模块...\r\n");
    ESP8266_SendCmd("AT+RST\r\n", "ready");  // 等待 10s
    HAL_Delay(4000);  // 多等 4s 确保重启完成

    printf("[ESP] 2. 设置 Station 模式...\r\n");
    ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK");

    printf("[ESP] 3. 开启 DHCP...\r\n");
    ESP8266_SendCmd("AT+CWDHCP=1,1\r\n", "OK");

    printf("[ESP] 4. 连接 WiFi...\r\n");
    char join_cmd[100];
    sprintf(join_cmd, "AT+CWJAP=\"yiyi\",\"1004991014\"\r\n");
    ESP8266_SendCmd(join_cmd, "GOT IP");  // WiFi 连接最长等 20s

    if (strstr((char *)esp8266_buf, "GOT IP") != NULL)
    {
        printf("[ESP] WiFi 连接成功！\r\n");
    }
    else
    {
        printf("[ESP] WiFi 连接失败，最后响应: %s\r\n", esp8266_buf);
    }

    // 如果你需要 MQTT，继续加，但先确保 WiFi 通
    printf("[ESP] 初始化结束\r\n");
}
void SendData(float temp,float humi)
{
char cmdbuf[512];
	ESP8266_Clear();
  sprintf(cmdbuf, "AT+MQTTPUB=0,\"%s\",\"{\\\"id\\\":\\\"123\\\"\\,\\\"params\\\":{\\\"temp\\\":{\\\"value\\\":%.1f}\\,\\\"humi\\\":{\\\"value\\\":%.1f}\\}}\",0,0\r\n",pubtopic, temp, humi);
while(ESP8266_SendCmd(cmdbuf,"OK"))
		delay_ms(500);
	memset(cmdbuf,0,sizeof(cmdbuf));
	delay_ms(100);
}




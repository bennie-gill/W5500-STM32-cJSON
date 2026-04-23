#include "control.h"
#include "tim.h"
#include "gpio.h"
#include "mqtt_app.h"
#include "RS485.h"
 Control_Device_t g_device ;
	 
 key_t key;

//========???????????========
void Control_lamp(uint8_t on)
{
	Send_to_Slave_write(on,REG_LAMP);
	printf("lamp:%s\r\n",on?"ON":"OFF");
}
void Control_fun(uint8_t on)
{
	Send_to_Slave_write(on,REG_FAN);
	printf ("fun:%s\r\n",on?"ON":"OFF");
}
void Control_spray(uint8_t on)
{
	Send_to_Slave_write(on,REG_SPRAY);
	printf ("SPRAY:%s\r\n",on?"ON":"OFF");
}
void Control_co2(uint8_t on)
{
	printf ("Alarm:%s\r\n",on?"ON":"OFF");
}

void key_scan(void)
{

	switch(key.count)
	{
		case 0:
	if(key.sta == 0)
	{
	key.count = 1;
	key.long_time = 0;
	}
	break;
		case 1:
			if(key.sta == 0)
			{
			key.count = 2;
			}
			else
				key.count = 0;
			break;
		case 2:
			
			if(key.sta == 1)
			{
				key.count = 0;
				if(key.long_time >= 3000)
				{
				key.long_long_flage = 1;
					printf("long long flage open\r\n");
				}
			else if(key.long_time >= 1000)
			{
			key.long_flage = 1;
				printf("long flage open\r\n");
			}
			else 
			{key.flage = 1;
			printf("flage open\r\n");
			}
			}
			else if(key.sta == 0)
			{
			key.long_time ++;
			}
			break;
	}
	
	
}

void key_control(void)
{
if(key.flage == 1)
{
key.flage = 0;
g_device.lamp_state = !g_device.lamp_state;
Control_lamp(g_device.lamp_state);
Send_Lamp_Command(g_device.lamp_state,0);
printf("Lamp toggled by KEY\n");
}

if(key.long_flage == 1)
{
key.long_flage = 0;
g_device.fun_state = !g_device.fun_state;
Control_fun(g_device.fun_state);
Send_fun_Command(g_device.fun_state,1);
printf("fun toggled by KEY\n");
}
if(key.long_long_flage == 1)
{
key.long_long_flage = 0;
g_device.spray_state = !g_device.spray_state;
Control_spray(g_device.spray_state);
Send_spray_Command(g_device.spray_state,2);
//MQTT_Publish_Status();
printf("spray toggled by KEY\n");
}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM4) {
    key.sta = HAL_GPIO_ReadPin(key_GPIO_Port, key_Pin);
    key_scan();
  }
}


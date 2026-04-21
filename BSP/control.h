#ifndef __CONTROL__
#define __CONTROL__
#include "main.h"
#include <stdio.h>
typedef struct 
{
	uint8_t lamp_state;
	uint8_t fun_state;
	uint8_t co2_alarm;
	uint8_t spray_state;
}Control_Device_t;
typedef struct Device{
uint8_t state;
uint8_t id;
}Device_t;

typedef struct Key{
	uint8_t count;
	_Bool flage;
	_Bool long_flage;
	_Bool long_long_flage;
	_Bool sta;
	uint16_t long_time;
}key_t;
extern Control_Device_t g_device ;
extern key_t key;
void Control_Init(void);
void Control_lamp(uint8_t state);
void Control_fun(uint8_t state);
void Control_spray(uint8_t state);
void Contorl_co2(uint8_t state);
void key_control(void);
#endif


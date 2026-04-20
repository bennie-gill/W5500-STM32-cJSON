#ifndef __SensorData_H
#define __SensorData_H
#include "main.h"
typedef struct	
{
	float temp;
	float humi;
	float smoke;
	uint8_t is_valid;	
	
}SensorData_t;
uint8_t Sensors_read_all(SensorData_t *data);
#endif

#ifndef __SENSOR_H
#define __SENSOR_H

#include "main.h"

#define CO2_THRESHOLD  1000

/* º¯ÊıÉùÃ÷ */
void Sensor_Init(void);
void Sensor_Read_TempHumi(float *temperature, float *humidity);
uint16_t Sensor_Read_CO2(void);
void Sensor_task(void);
#endif 

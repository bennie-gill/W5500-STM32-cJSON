#include "control.h"
void Control_init()
{
Control_lamp(0);
}
void Control_lamp(uint8_t state)
{
	printf("lamp:%s\r\n",state?"ON":"OFF");
}
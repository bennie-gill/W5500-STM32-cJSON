#include "SensorData.h"
#include "DHT11.h"
#include <stdlib.h>
uint8_t Sensors_read_all(SensorData_t *data)
{
	uint8_t success = 0;
	uint8_t dht_ok = 0;
	uint8_t mq_ok = 0;
	
	if (data == NULL) return 0;
	
	data->humi = -999.0f;
	data->temp = -999.0f;
	data->smoke = -999.0f;
	data->is_valid = 0;
	
	for(int retry = 0;retry < 3;retry++)
	{
		if(dht11_read_data(&data->temp,&data->humi))
		{
			dht_ok = 1;
			break;
		}
	
	}
static float simulated_smoke = 1200.0f;   // 初始值

simulated_smoke += (rand() % 100) - 50;   // 每次浮动 ±50
if (simulated_smoke < 500.0f) simulated_smoke = 500.0f;
if (simulated_smoke > 3000.0f) simulated_smoke = 3000.0f;

data->smoke = simulated_smoke;
mq_ok = 1;
	if (dht_ok && mq_ok &&
        data->temp > -40.0f && data->temp < 80.0f &&
        data->humi >= 0.0f && data->humi <= 100.0f &&
        data->smoke >= 0.0f && data->smoke < 4096.0f) {
        
        data->is_valid = 1;
        success = 1;
    }
	return success;

}


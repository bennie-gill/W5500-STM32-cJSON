#include "sensor.h"
#include "control.h"
#include "mqtt_app.h"
void Sensor_Init(void);
void Sensor_Read_TempHumi(float *temperature, float *humidity);
uint16_t Sensor_Read_CO2(void);

static float mock_temperature = 25.0f;
static float mock_humidity = 60.0f;
static uint16_t mock_co2 = 450;
extern void MQTT_Publish_Alert(uint16_t co2_value, uint8_t alarm);
static uint8_t last_alert = 0;

void Sensor_Read_TempHumi(float *temp, float *humi) {
  /* 模拟数据变化 */
  mock_temperature += 0.1f;
  if (mock_temperature > 30.0f)
    mock_temperature = 20.0f;

  mock_humidity += 0.5f;
  if (mock_humidity > 80.0f)
    mock_humidity = 40.0f;

  *temp = mock_temperature;
  *humi = mock_humidity;
}

uint16_t Sensor_Read_CO2(void) {
  static int direction = 1;

  // 1. 先判断方向，再执行加减，避免无符号数溢出
  if (direction == 1) {
    if (mock_co2 >= 1200) {
      direction = -1;
      mock_co2 -= 10;
    } else {
      mock_co2 += 10;
    }
  } else {
    if (mock_co2 <= 400) {
      direction = 1;
      mock_co2 += 10;
    } else {
      mock_co2 -= 10;
    }
  }
  return mock_co2;
}
extern Control_Device_t g_device;
static void CO2_Alert_Check(uint16_t co2) {
  uint8_t current_alert = (co2 > CO2_THRESHOLD) ? 1 : 0;
  if (current_alert != last_alert) {
    g_device.co2_alarm = current_alert;
    if (current_alert) {
      printf("CO2 ALERT! Value: %d ppm (Threshold: %d)\n", co2, CO2_THRESHOLD);
      MQTT_Publish_Alert(co2, g_device.co2_alarm);
      Contorl_co2(g_device.co2_alarm);
    } else {
      MQTT_Publish_Alert(co2, g_device.co2_alarm);
      Contorl_co2(0);
    }
    last_alert = current_alert;
  }
}

void Sensor_task(void) {
  static uint32_t last_read = 0;
  static uint32_t last_upload = 0;
  uint32_t now = HAL_GetTick();

  // 1秒读取一次传感器并发布
  if (now - last_read >= 1000) {
    uint16_t co2;
    co2 = Sensor_Read_CO2();

    // 【修改点】始终发布 CO2 数据，而不仅仅在报警状态变化时发布
    // 这样 MQTTX 就能持续看到 CO2 的实时数值
    MQTT_Publish_Alert(co2, (co2 > CO2_THRESHOLD) ? 1 : 0);

    CO2_Alert_Check(co2);

    printf("[Sensor] Periodically publishing temp/humi & CO2...\r\n");
    Publish_sensor_temp_humi();
    last_read = now;
  }

  // 2秒发布一次全设备状态
  if (now - last_upload >= 2000) {
    printf("[Sensor] Periodically publishing all status...\r\n");
    Send_All_Device_Status();
    last_upload = now;
  }
}
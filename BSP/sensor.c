#include "sensor.h"
#include "control.h"
#include "mqtt_app.h"
#include "DHT11.h"
#include "RS485.h"
static uint16_t mock_co2 = 450;
extern void MQTT_Publish_Alert(uint16_t co2_value, uint8_t alarm);
static uint8_t last_alert = 0;

void Sensor_Read_TempHumi(float *temp, float *humi) {
  if (temp == NULL || humi == NULL) {
    return;
  }

  ENV snapshot;
  uint32_t age_ms = 0xFFFFFFFFu;
  if (RS485_GetEnv(&snapshot, &age_ms, NULL) && age_ms != 0xFFFFFFFFu && age_ms < 5000u &&
      snapshot.temp != 0.0f && snapshot.hum != 0.0f) {
    *temp = snapshot.temp;
    *humi = snapshot.hum;
    return;
  }

  int err = dht11_read_data(temp, humi);
  if (!err) {
    printf("get temp humi sucess\r\n");
  }
}
uint16_t Sensor_Read_CO2(void) {
  static int direction = 1;
  ENV snapshot;
  uint32_t age_ms = 0xFFFFFFFFu;
  if (RS485_GetEnv(&snapshot, NULL, &age_ms) && age_ms != 0xFFFFFFFFu && age_ms < 5000u &&
      snapshot.co2 != 0) {
    return snapshot.co2;
  }

  // 1. ���жϷ�����ִ�мӼ��������޷��������
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
      Control_co2(g_device.co2_alarm);
    } else {
      MQTT_Publish_Alert(co2, g_device.co2_alarm);
      Control_co2(0);
    }
    last_alert = current_alert;
  }
}

void Sensor_task(void) {
  static uint32_t last_read = 0;
  static uint32_t last_upload = 0;
  static uint32_t last_rs485_poll = 0;
  uint32_t now = HAL_GetTick();

  if (now - last_rs485_poll >= 200) {
    RS485_Poll_Slaves();
    last_rs485_poll = now;
  }

  // 1���ȡһ�δ�����������
  if (now - last_read >= 1000) {
    uint16_t co2;
    co2 = Sensor_Read_CO2();

    // ���޸ĵ㡿ʼ�շ��� CO2 ���ݣ����������ڱ���״̬�仯ʱ����
    // ���� MQTTX ���ܳ������� CO2 ��ʵʱ��ֵ
    MQTT_Publish_Alert(co2, (co2 > CO2_THRESHOLD) ? 1 : 0);

    CO2_Alert_Check(co2);

    printf("[Sensor] Periodically publishing temp/humi & CO2...\r\n");
    Publish_sensor_temp_humi();
    last_read = now;
  }

  // 2�뷢��һ��ȫ�豸״̬
  if (now - last_upload >= 2000) {
    printf("[Sensor] Periodically publishing all status...\r\n");
    Send_All_Device_Status();
    last_upload = now;
  }
}


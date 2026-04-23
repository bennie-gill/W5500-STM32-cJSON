#ifndef __RS485_H
#define __RS485_H
#include "gpio.h"
#include "main.h"
#include "usart.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>


uint16_t Calculate_CRC16(uint8_t *data, uint16_t length);
void RS485_TX_MODE(void);
void RS485_RX_MODE(void);
void Modbus_Parse(uint8_t *rx_buf, uint16_t len);
void RS485_Poll_Slaves(void); // 轮询从机函数
void RS485_TxDone(void);      // DMA 发送完成处理
bool Send_to_Slave_read(uint16_t start, uint16_t num);
bool Send_to_Slave_write(uint8_t state, uint16_t reg);

#define DEVICE_ADDR (0X01U)
#define FUNC_WRITE (0X05U)
#define FUNC_READ (0X03U)
#define CONTR_OFF (0X00U)
#define CONTR_ON (0XFFU)

//========控制类=========
#define REG_LAMP (0X0000U)
#define REG_FAN (0X0001U)
#define REG_SPRAY (0X0002U)

//=======传感器============
#define REG_TEMP_HUM (0x0000U) // 温度
#define REG_CO2 (0x0002U)      // CO2

// ===== 环境数据 =====
typedef struct {
  float temp;
  float hum;
  uint16_t co2;

} ENV;
extern ENV env;
bool RS485_GetEnv(ENV *out, uint32_t *temp_humi_age_ms, uint32_t *co2_age_ms);
extern uint8_t CTRL_OK;
#endif /* __RS485_H */

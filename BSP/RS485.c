#include "RS485.h"

ENV env;
uint8_t CTRL_OK = 0;
// ===== 全局变量定义 =====
static uint8_t tx_buffer[8];                // DMA 发送缓冲，必须为静态或全局
static volatile bool rs485_tx_busy = false; // 发送忙标志
static volatile uint8_t poll_flag = 0;
static volatile uint32_t last_temp_humi_tick = 0;
static volatile uint32_t last_co2_tick = 0;

//======RS485输入输出==========
void RS485_TX_MODE(void) {
  HAL_GPIO_WritePin(RS485_GPIO_Port, RS485_Pin, GPIO_PIN_SET); // 发送使能
}

void RS485_RX_MODE(void) {
  HAL_GPIO_WritePin(RS485_GPIO_Port, RS485_Pin, GPIO_PIN_RESET); // 接收使能
}

bool Send_to_Slave_write(uint8_t state, uint16_t reg) {
  if (rs485_tx_busy) {
    return false; // 上一次发送尚未完成，避免重叠发送
  }

  tx_buffer[0] = DEVICE_ADDR;
  tx_buffer[1] = FUNC_WRITE;
  tx_buffer[2] = (uint8_t)(reg >> 8);
  tx_buffer[3] = (uint8_t)(reg & 0xFF);
  tx_buffer[4] = state ? CONTR_ON : CONTR_OFF;
  tx_buffer[5] = 0X00;
  uint16_t crc = Calculate_CRC16(tx_buffer, 6);
  tx_buffer[6] = crc & 0xff;
  tx_buffer[7] = crc >> 8;
  HAL_UART_AbortReceive(&huart2); // 停止当前 DMA 接收，避免与发送冲突
  RS485_TX_MODE();                // 切换到发送模式
  if (HAL_UART_Transmit_DMA(&huart2, tx_buffer, 8) != HAL_OK) {
    RS485_RX_MODE();
    HAL_UART_Receive_DMA(&huart2, rx_buffer, RX_BUFFER_SIZE);
    return false;
  }

  rs485_tx_busy = true;
  return true;
}
bool Send_to_Slave_read(uint16_t start, uint16_t num) {
  if (rs485_tx_busy) {
    return false;
  }
  tx_buffer[0] = DEVICE_ADDR;
  tx_buffer[1] = FUNC_READ;
  tx_buffer[2] = (uint8_t)(start >> 8);
  tx_buffer[3] = (uint8_t)(start & 0XFF);
  tx_buffer[4] = (uint8_t)(num >> 8);
  tx_buffer[5] = (uint8_t)(num & 0xFF);
  uint16_t crc = Calculate_CRC16(tx_buffer, 6);
  tx_buffer[6] = crc & 0xff;
  tx_buffer[7] = crc >> 8;
  HAL_UART_AbortReceive(&huart2);
  RS485_TX_MODE();
  if (HAL_UART_Transmit_DMA(&huart2, tx_buffer, 8) != HAL_OK) {
    RS485_RX_MODE();
    HAL_UART_Receive_DMA(&huart2, rx_buffer, RX_BUFFER_SIZE);
    return false;
  }
  rs485_tx_busy = true;
  return true;
}
//=============CRC计算===============
uint16_t Calculate_CRC16(uint8_t *data, uint16_t length) {
  uint16_t crc = 0xFFFF; // 初始值
  for (uint16_t i = 0; i < length; i++) {
    crc ^= data[i]; // 将数据与CRC寄存器异或
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001) { // 如果最低位为1
        crc >>= 1;        // 右移一位
        crc ^= 0xA001;    // 与多项式异或
      } else {
        crc >>= 1; // 直接右移一位
      }
    }
  }
  return crc;
}

//===========解析数据函数================
void Modbus_Parse(uint8_t *rx_buf, uint16_t len) {

  if (len < 5) {
    return;
  }
  if (rx_buf[0] != DEVICE_ADDR) {
    return;
  }

  //======CRC校验=======
  uint16_t receive_crc = rx_buf[len - 1] << 8 | rx_buf[len - 2];
  uint16_t crc = Calculate_CRC16(rx_buf, len - 2);

  if (receive_crc != crc)
    return;

  if (rx_buf[1] == FUNC_READ) {
    uint8_t byte_count = rx_buf[2];
    if ((uint16_t)(byte_count + 5) != len) {
      return;
    }
    if (byte_count == 4) {
      uint16_t t = (rx_buf[3] << 8) | rx_buf[4];
      uint16_t h = (rx_buf[5] << 8) | rx_buf[6];

      env.temp = t / 10.0f;
      env.hum = h / 10.0f;
      last_temp_humi_tick = HAL_GetTick();
    }
    if (byte_count == 2) {

      uint16_t c = (rx_buf[3] << 8) | rx_buf[4];
      env.co2 = c;
      last_co2_tick = HAL_GetTick();
    }
  } else if (rx_buf[1] == FUNC_WRITE) {
    if (len != 8) {
      return;
    }
    CTRL_OK = (rx_buf[4] == 0XFF);
  }
}

void RS485_TxDone(void) { rs485_tx_busy = false; }

bool RS485_GetEnv(ENV *out, uint32_t *temp_humi_age_ms, uint32_t *co2_age_ms) {
  if (out == NULL) {
    return false;
  }

  uint32_t now = HAL_GetTick();
  __disable_irq();
  *out = env;
  uint32_t th_tick = last_temp_humi_tick;
  uint32_t c_tick = last_co2_tick;
  __enable_irq();

  if (temp_humi_age_ms != NULL) {
    *temp_humi_age_ms = (th_tick == 0) ? 0xFFFFFFFFu : (now - th_tick);
  }
  if (co2_age_ms != NULL) {
    *co2_age_ms = (c_tick == 0) ? 0xFFFFFFFFu : (now - c_tick);
  }

  return true;
}

static void RS485_RequestTempHumi(void) {
  (void)Send_to_Slave_read(REG_TEMP_HUM, 2);
}
static void RS485_RequestCO2(void) { (void)Send_to_Slave_read(REG_CO2, 1); }

// 轮询读取
void RS485_Poll_Slaves(void) {
  if (rs485_tx_busy) {
    return; // 正在发送时跳过本次轮询，避免重复发送
  }
  if (poll_flag == 0) {
    poll_flag = 1;
    RS485_RequestTempHumi();
  } else {
    poll_flag = 0;
    RS485_RequestCO2();
  }
}

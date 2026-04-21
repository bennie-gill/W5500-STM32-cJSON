/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bsp_w5500.h"
#include "mpu6050.h"
#include "mqtt_app.h"
#include "socket.h"
#include "w5500.h"
#include "wizchip_conf.h"
#include <stdio.h>
#include "sensor.h"
#include "control.h"
extern void Sensor_task(void);
// 串口1重定�??

int fputc(int ch, FILE *f) {
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// UDP
#define SOCKET_UDP 0  // 使用 Socket 0
#define UDP_PORT 5000 // 监听端口5000
#define DATA_BUF_SIZE 2048
uint8_t g_udp_buf[DATA_BUF_SIZE];
int32_t udp_loopback(uint8_t sn, uint8_t *buf, uint16_t port) {
  int32_t ret;
  uint8_t status;
  uint16_t recv_size;
  uint8_t remote_ip[4];
  uint16_t remote_port;
  ret = getsockopt(sn, SO_STATUS, &status);

  if (ret != SOCK_OK) {
    printf("getsockopt status ERROR:%d\r\n", ret);
    return ret;
  }

  switch (status) {
  case SOCK_UDP:
    ret = getsockopt(sn, SO_RECVBUF, &recv_size);
    if (ret != SOCK_OK) {
      printf("getsockopt RECV_SIZE ERROR:%ld\r\n", (long)ret);
      return ret;
    }
    if (recv_size > 0) {
      if (recv_size > DATA_BUF_SIZE)
        recv_size = DATA_BUF_SIZE;
      ret = recvfrom(sn, buf, recv_size, remote_ip, &remote_port);
      if (ret > 0) {
        printf("UDP Recv: %ld bytes from %d.%d.%d.%d:%d\r\n", (long)ret,
               remote_ip[0], remote_ip[1], remote_ip[2], remote_ip[3],
               remote_port);

        // 可�?�：打印收到的数据（十六进制�?
        printf("Data: ");
        for (int i = 0; i < ret; i++) {
          printf("%02X ", buf[i]);
        }
        printf("\r\n");

        ret = sendto(sn, buf, ret, remote_ip, remote_port);
        if (ret < 0) {
          printf("UDP Send Error: %ld\r\n", (long)ret);
          return ret;
        }
        printf("UDP Send :%ld bytes send back\r\n", (long)ret);

      } else if (ret == SOCK_BUSY) {
      } else {
        printf("UDP Send Error: %ld\r\n", (long)ret);
        return ret;
      }
    }
    break;
  case SOCK_CLOSED:
    ret = socket(sn, Sn_MR_UDP, port, SF_IO_NONBLOCK);
    if (ret != sn) {
      printf("Socket Open Error: %ld\r\n", (long)ret);
      return ret;
    }
    printf("UDP Socket %d opened on port %d (Non-blocking mode)\r\n", sn, port);
    break;
  }
  return 1;
}
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TCP_SIZE_BUF 2048
#define SOCKET_TCP_SERVER 1
#define SOCKET_PORT 5001
uint8_t g_tcp_buffer[TCP_SIZE_BUF];
static uint8_t g_client_connected = 0;
int32_t tcp_server_task(uint8_t sn, uint8_t *buf, uint16_t port) {
  uint8_t tcp_status;
  int32_t ret;
  uint16_t recv_size;
  ret = getsockopt(sn, SO_STATUS, &tcp_status);
  if (ret != SOCK_OK) {
    return ret;
  }
  switch (tcp_status) {
  case SOCK_CLOSED: // 初始化TCP
                    //  打开 TCP Socket
    // 参数: (sn, 协议模式, 端口, 标志�?)
    // Sn_MR_TCP = 0x01, SF_IO_NONBLOCK = 0x40
    ret = socket(sn, Sn_MR_TCP, port, SF_IO_NONBLOCK);

    if (ret != sn) {
      return ret;
    }
    g_client_connected = 0;
    printf("[TCP] Socket %d opened, port %d\r\n", sn, port);
    break;
  case SOCK_INIT:
    printf("[TCP] Socket initialized, starting listen...\r\n");
    ret = listen(sn);
    if (ret != SOCK_OK) {
      return ret;
    }

    break;
  case SOCK_LISTEN: // 监听状�?�下不用做操�?
    break;
  case SOCK_ESTABLISHED: // 成功建立连接
    if (!g_client_connected) {
      uint8_t client_ip[4];
      uint16_t client_port;
      getSn_DIPR(sn, client_ip);
      client_port = getSn_DPORT(sn);
      printf("[TCP] Client connected: %d.%d.%d.%d:%d\r\n", client_ip[0],
             client_ip[1], client_ip[2], client_ip[3], client_port);

      g_client_connected = 1;
    }
    ret = getsockopt(sn, SO_RECVBUF, &recv_size);
    if (ret != SOCK_OK) {
      return ret;
    }
    if (recv_size > 0) {
      if (recv_size > TCP_SIZE_BUF)
        recv_size = TCP_SIZE_BUF;

      ret = recv(sn, buf, recv_size);
      if (ret > 0) {
        buf[ret] = '\0';
        printf("[TCP] Data: %s\r\n", buf);
        int32_t sent = 0;
        while (sent < ret) {
          int32_t tmp = send(sn, buf + sent, ret - sent);
          if (tmp < 0) {
            break;
          }
          sent += tmp;
        }
        if (sent == ret) {
          printf("[TCP] Sent back %ld bytes\r\n", (long)sent);
        }
      } else if (ret == SOCK_BUSY) {
      } else if (ret == SOCKERR_SOCKSTATUS) {
        // 连接已断�?
        printf("[TCP] Connection lost during recv\r\n");
        g_client_connected = 0;
      } else {
        printf("[TCP] recv() error: %ld\r\n", (long)ret);
      }
    }
    break;
  // 状�?? 5: SOCK_CLOSE_WAIT - 对端请求断开，等待本端关�?
  case SOCK_CLOSE_WAIT:
    g_client_connected = 0;
    ret = disconnect(sn);
    if (ret != SOCK_OK) {
      printf("[TCP] disconnect() failed: %ld\r\n", (long)ret);
      // 如果 disconnect 失败，强�? close
      close(sn);
    }

    break;
  case SOCK_LAST_ACK:
  case SOCK_FIN_WAIT:
    break;
  }

  return 1;
}
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

#define PHYCFGR_OPMDC_MASK 0x38 // bits 5-3
#define PHYCFGR_DPX (1 << 2)    // 双工状�??
#define PHYCFGR_SPD (1 << 1)    // 速度 (1=100M, 0=10M)
#define PHYCFGR_LNK (1 << 0)    // 链路状�??
uint8_t get_link_status(void) { return getPHYCFGR() & PHYCFGR_LNK; }
uint8_t get_link_speed(void) { return (getPHYCFGR() & PHYCFGR_SPD) ? 100 : 10; }
const char *get_full_duplex(void) {
  return (getPHYCFGR() & PHYCFGR_DPX) ? "full" : "half";
}
void ment_monitor_phy(void) {
  uint8_t last_link = 0;
  uint8_t current_link;
  int count = 0;
  while (count < 30) {
    current_link = get_link_status();
    printf("current_link:%d\r\n", current_link);
    if (current_link != last_link) {
      if (current_link) {
        printf("[%ds] LINK UP\r\n", count);
        printf("SPEED:%dMbps DUPLEX:%s\r\n", get_link_speed(),
               get_full_duplex());
      } else {
        printf("[%ds] LINK DOWN\r\n", count);
      }
      last_link = current_link;
    }
    HAL_Delay(1000);
    count++;
    printf("count:%d\r\n", count);
  }
}
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  MX_TIM4_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
  // MPU6050_Init();
  w5500_init();
  w5500_dump();
  //mqtt_init();

  wiz_NetInfo info;
  wizchip_getnetinfo(&info);
  // 通过串口打印出来
  printf("Current MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\r\n", info.mac[0],
         info.mac[1], info.mac[2], info.mac[3], info.mac[4], info.mac[5]);
  // wiz_NetTimeout Timer_out, Timer_out1;
  // Timer_out.retry_cnt = 5;
  // Timer_out.time_100us = 2000;
  // wizchip_settimeout(&Timer_out);
  // wizchip_gettimeout(&Timer_out1);
  // ment_monitor_phy();
  // printf("rtr:%d,rcr:%d\r\n", Timer_out1.time_100us, Timer_out1.retry_cnt);
	mqtt_init();
	HAL_Delay(500);
	if(!g_MQTTclient.isconnected)
	{
	 printf("\r\n[ERROR] MQTT connection failed!\r\n");
	 printf("Please check:\r\n");
	 printf("  - Mosquitto is running on PC (192.168.227.1:1883)\r\n");
	 printf("  - Network cable is connected\r\n");
	 printf("  - Firewall allows port 1883\r\n");
	}
	HAL_TIM_Base_Start_IT(&htim4);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
		mqtt_loop();
	  Sensor_task();
		key_control();
    // udp_loopback(SOCKET_UDP,g_udp_buf,UDP_PORT);
    // tcp_server_task(SOCKET_TCP_SERVER, g_tcp_buffer, SOCKET_PORT);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1) {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

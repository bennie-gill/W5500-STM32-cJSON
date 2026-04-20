#include "bsp_w5500.h"
#include "wizchip_conf.h"


#include <stdio.h>

static uint32_t g_wiz_primask = 0;
static uint32_t g_wiz_cris_depth = 0;

static void wiz_cris_enter(void) {
  if (g_wiz_cris_depth == 0) {
    g_wiz_primask = __get_PRIMASK();
    __disable_irq();
  }
  g_wiz_cris_depth++;
}

static void wiz_cris_exit(void) {
  if (g_wiz_cris_depth == 0) {
    return;
  }
  g_wiz_cris_depth--;
  if (g_wiz_cris_depth == 0 && g_wiz_primask == 0) {
    __enable_irq();
  }
}

static uint8_t wiz_spi_readbyte(void) { return Spi1_read_write_byte(0x00); }

static void wiz_spi_writebyte(uint8_t wb) { (void)Spi1_read_write_byte(wb); }
void CS_LOW(void) { HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET); }

void CS_HIGH(void) { HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET); }

uint8_t Spi1_read_write_byte(uint8_t byte) {
  uint8_t rec_data;
  HAL_SPI_TransmitReceive(&hspi1, &byte, &rec_data, 1, HAL_MAX_DELAY);
  return rec_data;
}
void network_init(void) {
  wiz_NetInfo net_info = {.mac = {0x00, 0x08, 0xdc, 0x11, 0x11, 0x11},
                          .ip = {192, 168, 227, 88}, // 改这里
                          .sn = {255, 255, 255, 0},
                          .gw = {192, 168, 227, 162}, // 网关
                          .dns = {8, 8, 8, 8},
                          .dhcp = NETINFO_STATIC};
  wizchip_setnetinfo(&net_info);
}

void w5500_init(void) {
  uint8_t txsize[8] = {2, 2, 2, 2, 2, 2, 2, 2};
  uint8_t rxsize[8] = {2, 2, 2, 2, 2, 2, 2, 2};

  reg_wizchip_cris_cbfunc(wiz_cris_enter, wiz_cris_exit);
  reg_wizchip_cs_cbfunc(CS_LOW, CS_HIGH);//注册片选控制函数
  reg_wizchip_spi_cbfunc(wiz_spi_readbyte, wiz_spi_writebyte);

  wizchip_init(txsize, rxsize);
  network_init();
}

void w5500_dump(void) {
  wiz_NetInfo info;
  int8_t link = -1;
  uint8_t ver = getVERSIONR();
  uint8_t mr = getMR();

  ctlwizchip(CW_GET_PHYLINK, (void *)&link);
  wizchip_getnetinfo(&info);

  printf("W5500 VERSIONR=0x%02X MR=0x%02X PHYLINK=%d\r\n", ver, mr, (int)link);
  printf("MAC=%02X:%02X:%02X:%02X:%02X:%02X\r\n", info.mac[0], info.mac[1],
         info.mac[2], info.mac[3], info.mac[4], info.mac[5]);
  printf("IP=%d.%d.%d.%d SN=%d.%d.%d.%d GW=%d.%d.%d.%d DNS=%d.%d.%d.%d "
         "DHCP=%d\r\n",
         info.ip[0], info.ip[1], info.ip[2], info.ip[3], info.sn[0], info.sn[1],
         info.sn[2], info.sn[3], info.gw[0], info.gw[1], info.gw[2], info.gw[3],
         info.dns[0], info.dns[1], info.dns[2], info.dns[3], (int)info.dhcp);
}

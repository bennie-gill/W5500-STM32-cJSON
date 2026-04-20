#ifndef __w5500_H
#define __w5500_H

#include "gpio.h"
#include "spi.h"

void CS_LOW(void);
void CS_HIGH(void);
uint8_t Spi1_read_write_byte(uint8_t byte);
void network_init(void);
void w5500_init(void);
void w5500_dump(void);
#endif

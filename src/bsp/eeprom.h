#ifndef _AT24C02_H_
#define _AT24C02_H_

#include "stm32f10x.h"

#define ROLL_ADDR_OFFSET  0 // Roll 微调值存储地址
#define PITCH_ADDR_OFFSET 4 // Pitch 微调值存储地址

uint8_t test_AT24C02(void);
void write_AT24C02(uint8_t addr, uint16_t data);
uint16_t read_AT24C02(uint8_t addr);

#endif

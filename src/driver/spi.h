/*******************************************************************
 * 软件模拟 SPI 驱动头文件（NRF24L01+ 专用）
 * 引脚：PB13 = SCK，PB14 = MISO，PB15 = MOSI（模式 0：CPOL=0, CPHA=0）
 ******************************************************************/
#ifndef _SPI_H_
#define _SPI_H_

#include "stm32f10x.h"

extern void SPI_Config(void);
extern u8 SPI_RW(u8 dat);

#endif /* _SPI_H_ */

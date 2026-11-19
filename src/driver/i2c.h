/*******************************************************************
 * 软件模拟 I2C 驱动头文件（挂载 AT24C02 EEPROM）
 * 引脚：PB6 = SCL，PB7 = SDA（开漏输出，外部上拉电阻）
 ******************************************************************/
#ifndef _I2C_H_
#define _I2C_H_

#include "stm32f10x.h"

#define IIC_GPIO GPIOB
#define SCL_PIN  GPIO_Pin_6
#define SDA_PIN  GPIO_Pin_7

#define SCL_H    (GPIOB->BSRR = GPIO_Pin_6)
#define SCL_L    (GPIOB->BRR = GPIO_Pin_6)
#define SDA_H    (GPIOB->BSRR = GPIO_Pin_7)
#define SDA_L    (GPIOB->BRR = GPIO_Pin_7)
#define SDA_read (GPIOB->IDR & GPIO_Pin_7)

/* 返回值约定：0 表示成功，1 表示失败 */
extern void IIC_Init(void);
extern int8_t IIC_Write_One_Byte(uint8_t addr, uint8_t reg, uint8_t data);
extern int8_t IIC_Write_Bytes(uint8_t addr, uint8_t reg, const uint8_t *data, uint8_t len);
extern int8_t IIC_read_Bytes(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len);

#endif /* _I2C_H_ */

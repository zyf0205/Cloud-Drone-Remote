#ifndef _I2C_H_
#define _I2C_H_

#include "stm32f10x.h"

/* 软件 I2C：PB6 = SCL, PB7 = SDA（开漏输出，板上有上拉电阻） */
#define IIC_GPIO  GPIOB
#define SCL_PIN   GPIO_Pin_6
#define SDA_PIN   GPIO_Pin_7

#define SCL_H GPIOB->BSRR = GPIO_Pin_6
#define SCL_L GPIOB->BRR  = GPIO_Pin_6
#define SDA_H GPIOB->BSRR = GPIO_Pin_7
#define SDA_L GPIOB->BRR  = GPIO_Pin_7

#define SDA_read (GPIOB->IDR & GPIO_Pin_7)

/* 返回 0 成功，1 失败 */
void IIC_Init(void);
int8_t IIC_Write_One_Byte(uint8_t addr, uint8_t reg, uint8_t data);
int8_t IIC_Read_One_Byte(uint8_t addr, uint8_t reg);
int8_t IIC_Write_Bytes(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len);
int8_t IIC_read_Bytes(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len);

#endif

#ifndef __USART1_H
#define __USART1_H

#include "stm32f10x.h"

void USART1_SendByte(const int8_t *Data, uint8_t len);
void USART1_Config(void);

#endif /* __USART1_H */

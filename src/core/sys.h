#ifndef _SYS_H___
#define _SYS_H___

#include "stm32f10x.h"
#include "delay.h"

/* 蜂鸣器（PA12，低电平有效） */
#define BEEP_H GPIOA->BSRR = GPIO_Pin_12 // 蜂鸣器响
#define BEEP_L GPIOA->BRR = GPIO_Pin_12  // 蜂鸣器关

extern void ALL_Init(void);

#endif

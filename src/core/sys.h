#ifndef __SYS_H
#define __SYS_H

#include "stm32f10x.h"
#include "delay.h"

/* 蜂鸣器（PA12）:BEEP_H 响 / BEEP_L 停 */
#define BEEP_H GPIOA->BSRR = GPIO_Pin_12
#define BEEP_L GPIOA->BRR  = GPIO_Pin_12

#endif

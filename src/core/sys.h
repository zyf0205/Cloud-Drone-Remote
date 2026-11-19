/*******************************************************************
 * 系统底层硬件快捷宏定义
 *
 * 蜂鸣器硬件说明：
 *   PA12 推挽输出，高电平导通发声（BSRR 置 1），低电平关断静音（BRR 置 0）。
 *   调度器启动后蜂鸣器引脚由 Task_Buzzer 唯一拥有；
 *   仅开机初始化及 FreeRTOS 致命错误钩子中可直接调用底层宏。
 ******************************************************************/
#ifndef __SYS_H
#define __SYS_H

#include "stm32f10x.h"
#include "delay.h"

/* 蜂鸣器底层寄存器操作宏（PA12：高电平响 / 低电平停） */
#define BEEP_ON  (GPIOA->BSRR = GPIO_Pin_12)
#define BEEP_OFF (GPIOA->BRR = GPIO_Pin_12)

#endif /* __SYS_H */

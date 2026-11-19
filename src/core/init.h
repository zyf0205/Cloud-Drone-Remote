/*******************************************************************
 * 系统时钟与板级硬件总初始化头文件（调度器启动前调用）
 ******************************************************************/
#ifndef __INIT_H
#define __INIT_H

#include "stm32f10x.h"

extern void RCC_Configuration(void);
extern void ALL_Init(void);

#endif /* __INIT_H */

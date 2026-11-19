/*******************************************************************
 * 系统硬件毫秒时钟与阻塞延时接口
 *
 * 使用约束：
 *  - SysTick 已交给 FreeRTOS 内核，硬件毫秒时钟由 TIM3（1kHz）独立提供。
 *  - delay_ms() 为忙等延时，仅限调度器启动前的硬件初始化阶段使用；
 *    FreeRTOS 任务内部请务必使用 vTaskDelay() / vTaskDelayUntil()。
 ******************************************************************/
#ifndef _DELAY_H
#define _DELAY_H

#include <stdint.h>

extern void cycleCounterInit(void);
extern uint32_t get_ms_tick(void);
extern void delay_ms(uint16_t nms);

#endif /* _DELAY_H */

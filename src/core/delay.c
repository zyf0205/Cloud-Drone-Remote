/*******************************************************************
 * 系统硬件毫秒时钟（TIM3 1kHz）与启动阶段忙等延时实现
 *
 * 设计说明：
 *  - TIM3 中断优先级为 6（高于 FreeRTOS 内核的 15，且不调用任何 FreeRTOS API），
 *    在调度器启动前后均可稳定提供精确 1ms 时基。
 *  - delay_ms() 使用局部变量记录起始时间戳，避免旧版静态变量不可重入的隐患。
 ******************************************************************/
#include "stm32f10x.h"
#include "delay.h"
#include "all_data.h"
#include "tim.h"

/* TIM3 更新中断：每 1ms 累加一次全局毫秒计数 */
void TIM3_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
        SysTick_count++;
    }
}

/* 初始化硬件毫秒时钟 */
void cycleCounterInit(void)
{
    TIM3_Config();
}

/* 读取当前硬件毫秒计数值（无符号 32 位，约 49.7 天回绕一次） */
uint32_t get_ms_tick(void)
{
    return SysTick_count;
}

/* 忙等毫秒延时（仅用于调度器启动前的初始化阶段） */
void delay_ms(uint16_t nms)
{
    uint32_t start = SysTick_count;

    /* 无符号减法天然免疫 32 位计数器溢出回绕 */
    while ((uint32_t)(SysTick_count - start) < (uint32_t)nms)
    {
    }
}

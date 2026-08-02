#include "stm32f10x.h"
#include "delay.h"
#include "all_data.h"
#include "tim.h"

/*
 * 系统毫秒时钟由 TIM3（1kHz）提供，SysTick 已交给 FreeRTOS 内核使用。
 * TIM3_IRQHandler 在调度器启动前即已工作，因此 ALL_Init 中的 delay_ms 依然可用。
 */
void TIM3_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
        SysTick_count++;
    }
}

void cycleCounterInit(void)
{
    TIM3_Config();
}

static uint32_t last_systick;
void delay_ms(uint16_t nms)
{
    last_systick = SysTick_count;
    while (SysTick_count - last_systick < nms)
        ;
}

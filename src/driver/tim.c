/*******************************************************************
 * TIM3 硬件毫秒时钟驱动（精确 1kHz 更新中断）
 *
 * 时钟树计算核对（STM32F103C8T6）：
 *   SYSCLK = 48MHz，HCLK = 48MHz，APB1 预分频 = /2 → PCLK1 = 24MHz。
 *   根据 STM32F10x 时钟树规则：当 APB 分频系数 != 1 时，挂在该总线上的
 *   定时器输入时钟自动 ×2，因此 TIM3CLK = 2 × PCLK1 = 48MHz。
 *   配置 PSC = 47（48 分频）→ 计数频率 1MHz（1us/步）；
 *   配置 ARR = 999（1000 步）→ 更新中断频率 = 1kHz（精确 1ms）。
 *
 * 中断优先级：
 *   Group4 下设为 6，不调用 FreeRTOS API，高于 SysTick(15)，保证毫秒精度。
 ******************************************************************/
#include "stm32f10x.h"
#include "tim.h"

#define TIM3_PRESCALER_1MHZ 47u  /* 48MHz / (47 + 1) = 1MHz */
#define TIM3_PERIOD_1KHZ    999u /* 1MHz / (999 + 1) = 1kHz (1ms) */

void TIM3_Config(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel                   = TIM3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_TimeBaseStructure.TIM_Period        = TIM3_PERIOD_1KHZ;
    TIM_TimeBaseStructure.TIM_Prescaler     = TIM3_PRESCALER_1MHZ;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    TIM_ClearFlag(TIM3, TIM_FLAG_Update);
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM3, ENABLE);
}

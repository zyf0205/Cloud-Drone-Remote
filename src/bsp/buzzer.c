/*******************************************************************
 * 蜂鸣器板级驱动实现（PA12）
 *
 * 硬件极性：高电平响（BEEP_ON = BSRR），低电平停（BEEP_OFF = BRR），
 * 与原板级电路驱动方式完全一致。
 ******************************************************************/
#include "buzzer.h"
#include "sys.h"

void Buzzer_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    Buzzer_Off();
}

void Buzzer_On(void)
{
    BEEP_ON;
}

void Buzzer_Off(void)
{
    BEEP_OFF;
}

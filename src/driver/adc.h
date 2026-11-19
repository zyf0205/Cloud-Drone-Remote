/*******************************************************************
 * ADC1 + DMA1 驱动头文件（4 通道摇杆后台循环采集）
 ******************************************************************/
#ifndef _ADC_DMA_CONFIG_H
#define _ADC_DMA_CONFIG_H

#include "stm32f10x.h"

/* ADC DMA 缓冲通道索引（与硬件引脚 PA0~PA3 一一对应） */
#define ADC_CH_YAW   0u /* PA0 -> ADC1_IN0 */
#define ADC_CH_THR   1u /* PA1 -> ADC1_IN1 */
#define ADC_CH_PITCH 2u /* PA2 -> ADC1_IN2 */
#define ADC_CH_ROLL  3u /* PA3 -> ADC1_IN3 */
#define ADC_CH_COUNT 4u

/* DMA 循环刷新的 12 位原始采样值（0~4095） */
extern __IO uint16_t ADC_ConvertedValue[ADC_CH_COUNT];

extern void ADC1_GPIO_Config(void);
extern void ADC1_Mode_Config(void);
extern void ADC1_ReadSnapshot(uint16_t out[ADC_CH_COUNT]);

#endif /* _ADC_DMA_CONFIG_H */

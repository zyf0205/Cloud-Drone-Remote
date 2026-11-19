/*******************************************************************
 * ADC1 + DMA1 循环采集驱动（4 路摇杆电位器）
 *
 * 硬件映射：
 *   PA0 = Yaw（通道 0）  PA1 = THR（通道 1）
 *   PA2 = Pitch（通道 2）PA3 = Roll（通道 3）
 *
 * 工作方式：
 *   ADC1 连续扫描 4 通道，DMA1_Channel1 循环将转换结果写入
 *   ADC_ConvertedValue[4]，CPU 无需轮询等待转换完成。
 ******************************************************************/
#include "adc.h"

#define ADC1_DR_Address ((u32)0x40012400 + 0x4c)

__IO uint16_t ADC_ConvertedValue[ADC_CH_COUNT];

/* 配置 PA0~PA3 为模拟输入，并打开 DMA1 / ADC1 / GPIOA 时钟 */
void ADC1_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/* 配置 DMA1 通道 1 循环搬运与 ADC1 四通道连续扫描，校准后启动转换 */
void ADC1_Mode_Config(void)
{
    DMA_InitTypeDef DMA_InitStructure;
    ADC_InitTypeDef ADC_InitStructure;

    /* 1. 配置 DMA1_Channel1：外设 ADC1->DR → 内存 ADC_ConvertedValue[4] */
    DMA_DeInit(DMA1_Channel1);
    DMA_InitStructure.DMA_PeripheralBaseAddr = ADC1_DR_Address;
    DMA_InitStructure.DMA_MemoryBaseAddr     = (u32)&ADC_ConvertedValue[0];
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize         = ADC_CH_COUNT;
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable; /* 外设地址固定 */
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;      /* 内存地址自增 */
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Circular; /* 循环搬运 */
    DMA_InitStructure.DMA_Priority           = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);
    DMA_Cmd(DMA1_Channel1, ENABLE);

    /* 2. 配置 ADC1：独立模式、4 通道扫描 + 连续转换、右对齐 */
    ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode       = ENABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel       = ADC_CH_COUNT;
    ADC_Init(ADC1, &ADC_InitStructure);

    /* ADC 时钟 = PCLK2(48MHz) / 8 = 6MHz（低于 14MHz 规格上限） */
    RCC_ADCCLKConfig(RCC_PCLK2_Div8);

    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 2, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 3, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 4, ADC_SampleTime_55Cycles5);

    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);

    /* 3. 上电硬件校准 */
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1))
    {
    }
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1))
    {
    }

    /* 4. 触发软件启动，此后后台自动持续刷新 */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

/* 拷贝一份 4 通道 ADC 快照（Cortex-M3 单次 16 位半字读取天然原子） */
void ADC1_ReadSnapshot(uint16_t out[ADC_CH_COUNT])
{
    uint8_t i;

    for (i = 0; i < ADC_CH_COUNT; i++)
    {
        out[i] = ADC_ConvertedValue[i];
    }
}

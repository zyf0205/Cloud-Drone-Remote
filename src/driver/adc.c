/*******************************************************************
 * ADC + DMA：4 通道摇杆采集
 * PA0 = Yaw, PA1 = THR, PA2 = Pitch, PA3 = Roll（ADC 通道 0~3）
 * DMA1_Channel1 循环模式，ADC_ConvertedValue[] 自动刷新
 ******************************************************************/
#include "adc.h"

#define ADC1_DR_Address ((u32)0x40012400 + 0x4c)

__IO uint16_t ADC_ConvertedValue[4];

/* ADC 引脚初始化：PA0~PA3 模拟输入 */
void ADC1_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/* ADC1 DMA 循环采集 4 通道，DMA 自动刷新内存，无需手动读取 */
void ADC1_Mode_Config(void)
{
    DMA_InitTypeDef DMA_InitStructure;
    ADC_InitTypeDef ADC_InitStructure;

    /* DMA 通道 1：外设 ADC1 数据寄存器 → 内存 ADC_ConvertedValue[4] */
    DMA_DeInit(DMA1_Channel1);
    DMA_InitStructure.DMA_PeripheralBaseAddr = ADC1_DR_Address;
    DMA_InitStructure.DMA_MemoryBaseAddr     = (u32)&ADC_ConvertedValue;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize         = 4;
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable; // 外设地址固定
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;      // 内存地址自增
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Circular; // 循环采集
    DMA_InitStructure.DMA_Priority           = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);
    DMA_Cmd(DMA1_Channel1, ENABLE);

    /* ADC1 独立模式、扫描 + 连续转换、右对齐 */
    ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode       = ENABLE; // 多通道扫描
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel       = 4;
    ADC_Init(ADC1, &ADC_InitStructure);

    /* ADC 时钟 = PCLK2/8 = 6MHz（不超过 14MHz 上限） */
    RCC_ADCCLKConfig(RCC_PCLK2_Div8);

    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 2, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 3, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 4, ADC_SampleTime_55Cycles5);

    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);

    /* ADC 校准 */
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));

    /* 启动软件转换，此后 DMA 循环刷新数据 */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

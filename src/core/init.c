#include "all_define.h"

volatile uint32_t SysTick_count;

/*
 * 系统时钟配置：遥控器要求功耗低，无外部晶振，
 * 使用内部 HSI（8MHz/2 * 12 = 48MHz）作为系统时钟。
 * 注意：SystemCoreClock 变量仍为编译期默认值，FreeRTOS 的
 * configCPU_CLOCK_HZ 已硬编码为 48MHz。
 */
void RCC_Configuration(void)
{
    RCC_DeInit(); // 将 RCC 寄存器设置为默认值

    RCC_HSICmd(ENABLE); // 使能 HSI
    while (RCC_GetFlagStatus(RCC_FLAG_HSIRDY) == RESET)
        ; // 等待 HSI 使能成功

    // 等待内部时钟,使能预取缓冲区
    FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
    FLASH_SetLatency(FLASH_Latency_2);

    RCC_HCLKConfig(RCC_SYSCLK_Div1);
    RCC_PCLK1Config(RCC_HCLK_Div2);
    RCC_PCLK2Config(RCC_HCLK_Div1);

    // 设置 PLL 时钟源及倍频系数 48MHz
    RCC_PLLConfig(RCC_PLLSource_HSI_Div2, RCC_PLLMul_12);
    RCC_PLLCmd(ENABLE);
    while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
        ;

    // 设置系统时钟 SYSCLK 为 PLL
    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
    while (RCC_GetSYSCLKSource() != 0x08)
        ;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA
                               | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC
                               | RCC_APB2Periph_ADC1 | RCC_APB2Periph_AFIO
                               | RCC_APB2Periph_SPI1,
                           ENABLE);

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3 | RCC_APB1Periph_I2C1 | RCC_APB1Periph_TIM3, ENABLE);
}

/* 蜂鸣器初始化（PA12，低电平响） */
void BEEP_INIT(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;       // PA12
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    BEEP_H;
}

/*
 * 全部硬件初始化（调度器启动前完成，delay_ms 可用）：
 * 软件 I2C（挂 AT24C02 EEPROM）→ NRF24L01 → EEPROM 自检 → ADC DMA（4 通道摇杆）
 * → 遥控通道与按键初始化。逻辑与旧版裸机完全一致。
 */
void ALL_Init(void)
{
    BEEP_INIT(); // 蜂鸣器初始化
    {
        int32_t beep_cnt = 2000000;
        while (beep_cnt-- > 0)
        {
        }
        BEEP_L; // 上电短暂提示音
    }

    IIC_Init();        // 软件 I2C 初始化
    NRF24L01_init();   // NRF24L01 初始化（TX 模式）
    while (test_AT24C02() != 0)
        ; // EEPROM 自检失败则等待

    ADC1_GPIO_Config(); // ADC 引脚初始化
    ADC1_Mode_Config(); // ADC DMA 循环采集 4 通道摇杆

    RC_INIT(); // 遥控通道与按键初始化
}

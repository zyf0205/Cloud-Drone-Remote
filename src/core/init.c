/*******************************************************************
 * 系统时钟配置与板级外设总初始化（调度器启动前执行）
 *
 * 时钟树（无外部晶振，低成本低功耗设计）：
 *   HSI(8MHz) / 2 × PLL12 = SYSCLK 48MHz
 *   AHB  = 48MHz，APB2 = 48MHz，APB1 = 24MHz（TIM3CLK = 2 × APB1 = 48MHz）
 *   system_stm32f10x.c 已定义 SYSCLK_FREQ_48MHz，SystemCoreClock = 48MHz。
 ******************************************************************/
#include "all_define.h"

volatile uint32_t SysTick_count = 0u;
volatile uint8_t g_boot_fault   = 0u;

#define EEPROM_INIT_RETRY_MAX 20u /* EEPROM 上电自检最多重试次数 */
#define BOOT_BEEP_MS          200u

/* 配置系统时钟为内部 HSI PLL 48MHz 并使能基础总线外设时钟 */
void RCC_Configuration(void)
{
    RCC_DeInit();

    RCC_HSICmd(ENABLE);
    while (RCC_GetFlagStatus(RCC_FLAG_HSIRDY) == RESET)
    {
    }

    FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
    FLASH_SetLatency(FLASH_Latency_2);

    RCC_HCLKConfig(RCC_SYSCLK_Div1); /* HCLK  = 48MHz */
    RCC_PCLK1Config(RCC_HCLK_Div2);  /* PCLK1 = 24MHz（TIM2/3/4 实际输入 48MHz） */
    RCC_PCLK2Config(RCC_HCLK_Div1);  /* PCLK2 = 48MHz */

    RCC_PLLConfig(RCC_PLLSource_HSI_Div2, RCC_PLLMul_12); /* 4MHz × 12 = 48MHz */
    RCC_PLLCmd(ENABLE);
    while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
    {
    }

    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
    while (RCC_GetSYSCLKSource() != 0x08u)
    {
    }

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_ADC1 | RCC_APB2Periph_AFIO,
                           ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
}

/*
 * 全部硬件上电初始化（调度器启动前完成，此时 TIM3 已工作，delay_ms 精确可用）：
 *   1. 蜂鸣器上电短鸣 200ms；
 *   2. 软件 I2C 初始化；
 *   3. NRF24L01+ 初始化与在位检测（带 20 次重试上界，失败置 FAULT_NRF）；
 *   4. AT24C02 EEPROM 自检（带 20 次重试上界，失败置 FAULT_EEPROM）；
 *   5. ADC1 + DMA1 4 通道循环采集启动；
 *   6. 遥控状态中心（加载合法微调值）与 8 路按键初始化。
 */
void ALL_Init(void)
{
    uint8_t retry;

    /* 1. 蜂鸣器初始化并输出 200ms 上电提示音 */
    Buzzer_Init();
    Buzzer_On();
    delay_ms(BOOT_BEEP_MS);
    Buzzer_Off();

    /* 2. 软件 I2C 初始化 */
    IIC_Init();

    /* 3. NRF24L01+ 上电检测与 TX 模式配置 */
    if (NRF24L01_init() != 0u)
    {
        g_boot_fault |= FAULT_NRF;
    }

    /* 4. AT24C02 EEPROM 自检（失败不卡死，置故障位交由 Task_Buzzer 报警） */
    retry = EEPROM_INIT_RETRY_MAX;
    while (test_AT24C02() != 0u && retry > 0u)
    {
        retry--;
    }
    if (retry == 0u)
    {
        g_boot_fault |= FAULT_EEPROM;
    }

    /* 5. 4 路摇杆 ADC + DMA 后台循环采集 */
    ADC1_GPIO_Config();
    ADC1_Mode_Config();

    /* 6. 加载已存微调（带合法性校验）并初始化 8 路按键 */
    RC_INIT();
}

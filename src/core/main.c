#include "all_define.h"
#include "app_tasks.h"
#include "stm32f10x_dbgmcu.h"

/*
 * FreeRTOS 版本主入口：
 * 1. NVIC 分组 Group4（FreeRTOS 内核要求 4 位抢占优先级）
 * 2. 系统时钟 48MHz（HSI PLL，遥控器无外部晶振）
 * 3. TIM3 提供 1ms 系统毫秒时钟（SysTick 归 FreeRTOS）
 * 4. 硬件初始化（调度器启动前完成，delay_ms 可用）
 * 5. 创建任务后启动调度器
 */
int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    RCC_Configuration(); // HSI PLL 48MHz
    cycleCounterInit();  // TIM3 1kHz 毫秒时钟

    /* 调试器断点停机时冻结 IWDG(看门狗在 RCTask 内启动),避免调试中被复位 */
    DBGMCU_Config(DBGMCU_IWDG_STOP, ENABLE);

    ALL_Init();

    xTaskCreate(RCTask, "rc", 128, NULL, TASK_PRIO_RC, NULL);

    vTaskStartScheduler();

    while (1);
}

/*******************************************************************
 * Cloud-Drone-Remote 固件主入口（FreeRTOS V9.0.0 多任务版）
 *
 * 启动顺序：
 *   1. 配置 NVIC 中断优先级分组 Group4（FreeRTOS 强制要求 4 位抢占优先级）
 *   2. 配置系统主频 48MHz（内部 HSI PLL，无外部晶振）
 *   3. 启动 TIM3 精确 1kHz 硬件毫秒时钟（SysTick 留给 FreeRTOS 内核）
 *   4. 配置调试停机冻结 IWDG，避免仿真器单步/断点期间触发看门狗复位
 *   5. 执行板级硬件上电初始化与外设自检（ALL_Init）
 *   6. 创建全部 FreeRTOS 队列与 5 个任务（App_TasksInit）并启动调度器
 ******************************************************************/
#include "all_define.h"
#include "app_tasks.h"
#include "stm32f10x_dbgmcu.h"

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    RCC_Configuration(); /* HSI PLL 48MHz */
    cycleCounterInit();  /* TIM3 精确 1kHz 毫秒时钟 */

    /* 调试器断点停机时冻结独立看门狗（看门狗在 Task_Monitor 内开启） */
    DBGMCU_Config(DBGMCU_IWDG_STOP, ENABLE);

    /* 板级外设初始化与上电自检 */
    ALL_Init();

    /* 创建全部 FreeRTOS 队列与多任务；创建失败则长鸣停机 */
    if (App_TasksInit() != pdPASS)
    {
        Buzzer_On();
        for (;;)
        {
        }
    }

    /* 启动 FreeRTOS 抢占式调度器（正常情况下永不返回） */
    vTaskStartScheduler();

    for (;;)
    {
    }
}

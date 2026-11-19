/*******************************************************************
 * FreeRTOS 系统钩子函数实现
 *
 * 包含：
 *   1. vApplicationStackOverflowHook : 任务栈溢出（方法 2 模式检测）→ 停机长鸣
 *   2. vApplicationMallocFailedHook  : 堆内存分配失败 → 停机长鸣
 *   3. vApplicationIdleHook          : 空闲任务进入 __WFI() 睡眠降功耗
 ******************************************************************/
#include "FreeRTOS.h"
#include "task.h"
#include "sys.h"

/* 任务栈溢出钩子（需 FreeRTOSConfig.h 中 configCHECK_FOR_STACK_OVERFLOW > 0） */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    taskDISABLE_INTERRUPTS();
    BEEP_ON; /* 立即长鸣告警，随后由 IWDG 超时复位恢复或供调试器定格排查 */
    for (;;)
    {
    }
}

/* 堆分配失败钩子（需 FreeRTOSConfig.h 中 configUSE_MALLOC_FAILED_HOOK = 1） */
void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    BEEP_ON;
    for (;;)
    {
    }
}

/*
 * 空闲任务钩子：无就绪业务任务时使 Cortex-M3 进入 WFI 睡眠模式。
 * 1ms SysTick / TIM3 中断或外设事件会自动唤醒 CPU，遥控器电池供电收益直接。
 */
void vApplicationIdleHook(void)
{
    __WFI();
}

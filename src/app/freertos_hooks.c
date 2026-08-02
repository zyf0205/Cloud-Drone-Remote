#include "FreeRTOS.h"
#include "task.h"
#include "sys.h"

/*
 * FreeRTOS 错误钩子：
 * 任何异常（堆栈溢出 / 内存分配失败）立即长鸣蜂鸣器提示，
 * 与旧版裸机相比保持"异常可见"，便于现场排查。
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, signed char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    BEEP_H; // 蜂鸣器长鸣
}

void vApplicationMallocFailedHook(void)
{
    BEEP_H;
}

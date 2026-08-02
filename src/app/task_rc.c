#include "all_define.h"
#include "app_tasks.h"
#include "remote.h"

/*
 * 遥控任务：10ms 周期（与旧版裸机 SysTick 分频调度一致）。
 * 采集 ADC 摇杆 → 滤波钳位 → 按键扫描 → NRF24L01 发送 32 字节帧。
 */
void RCTask(void *param)
{
    TickType_t lastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(10));
        RC_Analy();
    }
}

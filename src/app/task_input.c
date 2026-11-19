/*******************************************************************
 * Task_Input — 按键扫描与人机交互任务（优先级 3，20ms 周期）
 *
 * 单一硬件所有权：唯一读取 8 路按键 GPIO 并驱动按键去抖状态机的任务。
 *
 * 按键功能映射（单键按下边沿触发，松手后解锁下一次）：
 *   - AUX1 (PA15) : 飞控定高开关（1000 ↔ 2000）
 *   - AUX2 (PB11) : 飞控光流定点开关（1000 ↔ 2000）
 *   - AUX3 (PC14) : 飞控陀螺校准触发（1000 ↔ 2000）
 *   - AUX4 (PC15) : 预留辅助开关（1000 ↔ 2000）
 *   - 左/右微调 (PA6/PA7) : Roll 中位 -5 / +5（限幅 ±250，异步写 EEPROM）
 *   - 前/后微调 (PB0/PB1) : Pitch 中位 -5 / +5（限幅 ±250，异步写 EEPROM）
 ******************************************************************/
#include "app_tasks.h"
#include "key.h"
#include "remote.h"

/* 处理一次微调动作：更新内存状态 → 投递异步落盘 → 触发对应蜂鸣音效 */
static void handle_trim_key(uint8_t is_pitch, int16_t delta)
{
    rc_trim_t snap;

    if (RC_AdjustTrim(is_pitch, delta, &snap))
    {
        App_PostTrimSave(&snap);
        App_PostBuzzerEvent(BEEP_EVT_KEY_CLICK);
    }
    else
    {
        /* 已达 ±250 限幅边界：发双短促提示音提醒飞手 */
        App_PostBuzzerEvent(BEEP_EVT_TRIM_LIMIT);
    }
}

void Task_Input(void *param)
{
    TickType_t last_wake = xTaskGetTickCount();

    (void)param;

    for (;;)
    {
        key_event_t evt;

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TASK_PERIOD_INPUT_MS));

        evt = Key_Scan();
        switch (evt)
        {
            case KEY_EVT_AUX1:
                (void)RC_ToggleAux(RC_AUX_IDX_1);
                App_PostBuzzerEvent(BEEP_EVT_KEY_CLICK);
                break;

            case KEY_EVT_AUX2:
                (void)RC_ToggleAux(RC_AUX_IDX_2);
                App_PostBuzzerEvent(BEEP_EVT_KEY_CLICK);
                break;

            case KEY_EVT_AUX3:
                (void)RC_ToggleAux(RC_AUX_IDX_3);
                App_PostBuzzerEvent(BEEP_EVT_KEY_CLICK);
                break;

            case KEY_EVT_AUX4:
                (void)RC_ToggleAux(RC_AUX_IDX_4);
                App_PostBuzzerEvent(BEEP_EVT_KEY_CLICK);
                break;

            case KEY_EVT_TRIM_LEFT:
                handle_trim_key(0u, -RC_TRIM_STEP);
                break;

            case KEY_EVT_TRIM_RIGHT:
                handle_trim_key(0u, RC_TRIM_STEP);
                break;

            case KEY_EVT_TRIM_FRONT:
                handle_trim_key(1u, -RC_TRIM_STEP);
                break;

            case KEY_EVT_TRIM_BACK:
                handle_trim_key(1u, RC_TRIM_STEP);
                break;

            case KEY_EVT_NONE:
            default:
                break;
        }

        /* 上报本周期存活心跳 */
        App_NotifyHeartbeat(HEARTBEAT_BIT_INPUT);
    }
}

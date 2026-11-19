/*******************************************************************
 * Task_Buzzer — 声光反馈与故障报警统一管理任务（优先级 1）
 *
 * 单一硬件所有权：调度器启动后唯一允许控制蜂鸣器 PA12 的任务，
 * 彻底解决旧版按键提示音与故障蜂鸣互相覆盖/截断的问题。
 *
 * 声型约定：
 *   - 按键确认音（BEEP_EVT_KEY_CLICK）  ：单短鸣 40ms
 *   - 微调限幅音（BEEP_EVT_TRIM_LIMIT） ：双短促 30ms + 40ms停 + 30ms
 *   - NRF 故障报警（高优先，禁止起飞）  ：每秒 1 长鸣（400ms 响 / 600ms 停）
 *   - EEPROM 故障报警（仍可飞行）       ：每秒 2 短鸣（100ms 响 / 100ms 停 / 100ms 响 / 700ms 停）
 ******************************************************************/
#include "app_tasks.h"
#include "buzzer.h"

#define BEEP_IDLE_POLL_MS   200u /* 无故障时队列等待间隔，便于及时感知运行期新故障 */
#define BEEP_CLICK_ON_MS    40u
#define BEEP_CLICK_GAP_MS   20u
#define BEEP_LIMIT_ON_MS    30u
#define BEEP_LIMIT_GAP_MS   40u
#define BEEP_NRF_ON_MS      400u
#define BEEP_NRF_OFF_MS     600u
#define BEEP_EEPROM_ON_MS   100u
#define BEEP_EEPROM_GAP_MS  100u
#define BEEP_EEPROM_OFF_MS  700u

/* 播放一次瞬态交互提示音 */
static void buzzer_play_event(buzzer_event_t evt)
{
    switch (evt)
    {
        case BEEP_EVT_KEY_CLICK:
            Buzzer_On();
            vTaskDelay(pdMS_TO_TICKS(BEEP_CLICK_ON_MS));
            Buzzer_Off();
            vTaskDelay(pdMS_TO_TICKS(BEEP_CLICK_GAP_MS));
            break;

        case BEEP_EVT_TRIM_LIMIT:
            Buzzer_On();
            vTaskDelay(pdMS_TO_TICKS(BEEP_LIMIT_ON_MS));
            Buzzer_Off();
            vTaskDelay(pdMS_TO_TICKS(BEEP_LIMIT_GAP_MS));
            Buzzer_On();
            vTaskDelay(pdMS_TO_TICKS(BEEP_LIMIT_ON_MS));
            Buzzer_Off();
            vTaskDelay(pdMS_TO_TICKS(BEEP_CLICK_GAP_MS));
            break;

        case BEEP_EVT_NONE:
        default:
            break;
    }
}

/*
 * 在故障报警静音间隙中等待指定时长，期间若有瞬态按键音效到达则立即响应播放。
 */
static void buzzer_wait_silence_with_events(uint32_t silence_ms)
{
    TickType_t start_tick = xTaskGetTickCount();
    TickType_t total_ticks = pdMS_TO_TICKS(silence_ms);

    for (;;)
    {
        TickType_t elapsed = xTaskGetTickCount() - start_tick;
        buzzer_event_t evt;

        if (elapsed >= total_ticks)
            break;

        if (xQueueReceive(g_queue_buzzer, &evt, total_ticks - elapsed) == pdPASS)
        {
            buzzer_play_event(evt);
        }
        else
        {
            break;
        }
    }
}

void Task_Buzzer(void *param)
{
    buzzer_event_t evt;

    (void)param;

    for (;;)
    {
        /* 1. 优先响应队列中的所有瞬态交互提示音 */
        if (xQueueReceive(g_queue_buzzer, &evt, 0u) == pdPASS)
        {
            buzzer_play_event(evt);
            continue;
        }

        /* 2. 无瞬态事件时，根据系统故障位决定休眠等待或播放周期报警 */
        if (g_boot_fault & FAULT_NRF)
        {
            Buzzer_On();
            vTaskDelay(pdMS_TO_TICKS(BEEP_NRF_ON_MS));
            Buzzer_Off();
            buzzer_wait_silence_with_events(BEEP_NRF_OFF_MS);
        }
        else if (g_boot_fault & FAULT_EEPROM)
        {
            Buzzer_On();
            vTaskDelay(pdMS_TO_TICKS(BEEP_EEPROM_ON_MS));
            Buzzer_Off();
            vTaskDelay(pdMS_TO_TICKS(BEEP_EEPROM_GAP_MS));
            Buzzer_On();
            vTaskDelay(pdMS_TO_TICKS(BEEP_EEPROM_ON_MS));
            Buzzer_Off();
            buzzer_wait_silence_with_events(BEEP_EEPROM_OFF_MS);
        }
        else
        {
            /* 系统健康且空闲：阻塞等待新事件，CPU 自动进入 Idle WFI 休眠 */
            if (xQueueReceive(g_queue_buzzer, &evt, pdMS_TO_TICKS(BEEP_IDLE_POLL_MS)) == pdPASS)
            {
                buzzer_play_event(evt);
            }
        }
    }
}

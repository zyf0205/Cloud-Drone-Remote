/*******************************************************************
 * FreeRTOS 应用层统一装配与任务间通信封装
 *
 * 职责：
 *   1. 在调度器启动前一次性创建全部消息队列与 5 个业务/守护任务；
 *   2. 封装任务通知心跳、覆写存储队列与蜂鸣事件队列的投递逻辑，
 *      让各任务无需关心底层队列句柄判空与参数细节。
 ******************************************************************/
#include "app_tasks.h"

#define STORAGE_QUEUE_LEN 1u /* 覆写队列：仅保留最新微调快照，自动合并连按 */
#define BUZZER_QUEUE_LEN  8u /* 事件队列：缓存短时间内连续按键提示音 */

TaskHandle_t g_task_rc_tx   = NULL;
TaskHandle_t g_task_input   = NULL;
TaskHandle_t g_task_storage = NULL;
TaskHandle_t g_task_monitor = NULL;
TaskHandle_t g_task_buzzer  = NULL;

QueueHandle_t g_queue_storage = NULL;
QueueHandle_t g_queue_buzzer  = NULL;

volatile app_stack_hwm_t g_stack_hwm = {0, 0, 0, 0, 0};

/* 创建全部 IPC 队列与 FreeRTOS 任务 */
BaseType_t App_TasksInit(void)
{
    g_queue_storage = xQueueCreate(STORAGE_QUEUE_LEN, sizeof(rc_trim_t));
    g_queue_buzzer  = xQueueCreate(BUZZER_QUEUE_LEN, sizeof(buzzer_event_t));
    if (g_queue_storage == NULL || g_queue_buzzer == NULL)
        return pdFAIL;

    if (xTaskCreate(Task_RC_Tx, "rc_tx", TASK_STACK_RC_TX, NULL, TASK_PRIO_RC_TX, &g_task_rc_tx) != pdPASS)
        return pdFAIL;

    if (xTaskCreate(Task_Input, "input", TASK_STACK_INPUT, NULL, TASK_PRIO_INPUT, &g_task_input) != pdPASS)
        return pdFAIL;

    if (xTaskCreate(Task_Storage, "storage", TASK_STACK_STORAGE, NULL, TASK_PRIO_STORAGE, &g_task_storage) != pdPASS)
        return pdFAIL;

    if (xTaskCreate(Task_Buzzer, "buzzer", TASK_STACK_BUZZER, NULL, TASK_PRIO_BUZZER, &g_task_buzzer) != pdPASS)
        return pdFAIL;

    if (xTaskCreate(Task_Monitor, "monitor", TASK_STACK_MONITOR, NULL, TASK_PRIO_MONITOR, &g_task_monitor) != pdPASS)
        return pdFAIL;

    return pdPASS;
}

/* 核心任务向监控任务上报心跳位（零开销任务通知，按位或累加） */
void App_NotifyHeartbeat(uint32_t task_bit)
{
    if (g_task_monitor != NULL)
    {
        (void)xTaskNotify(g_task_monitor, task_bit, eSetBits);
    }
}

/* 投递最新微调快照给存储任务（覆写语义：连按多次只落盘最终值，保护 EEPROM 寿命） */
void App_PostTrimSave(const rc_trim_t *trim)
{
    if (g_queue_storage != NULL && trim != NULL)
    {
        (void)xQueueOverwrite(g_queue_storage, trim);
    }
}

/* 非阻塞投递蜂鸣器提示音事件（队列满时静默丢弃，绝不阻塞按键任务） */
void App_PostBuzzerEvent(buzzer_event_t evt)
{
    if (g_queue_buzzer != NULL && evt != BEEP_EVT_NONE)
    {
        (void)xQueueSend(g_queue_buzzer, &evt, 0u);
    }
}

/* 线程安全置位系统故障标志
 * g_boot_fault 是多个任务共享的读-改-写变量（RCTask 置 NRF、StorageTask 置 EEPROM），
 * 不加保护时低优先级任务的写入会被高优先级任务覆盖，导致故障报警丢失。 */
void App_SetBootFault(uint8_t mask)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        taskENTER_CRITICAL();
        g_boot_fault |= mask;
        taskEXIT_CRITICAL();
    }
    else
    {
        g_boot_fault |= mask;
    }
}

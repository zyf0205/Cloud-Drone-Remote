/*******************************************************************
 * Task_Monitor — 多任务健康监控与 IWDG 看门狗守护任务（优先级 2，500ms）
 *
 * 单一硬件所有权：唯一启动并喂养 STM32 独立看门狗（IWDG）的任务。
 *
 * 多任务看门狗原理（相比单任务无脑喂狗更可靠）：
 *   1. IWDG 参数：LSI 标称 40kHz，分频 128、重载 800 → 超时约 2.56s；
 *      远快于飞控端 5s 失联保护门限，空中异常复位后可自动恢复链路。
 *   2. Task_RC_Tx（10ms）与 Task_Input（20ms）每周期通过零开销的
 *      Task Notification（eSetBits）向本任务置位各自的心跳标志；
 *   3. 本任务每 500ms 唤醒一次，原子读取并清零过去 500ms 收集到的心跳位：
 *      - 仅当全部核心任务均成功签到（HEARTBEAT_ALL_MASK）时才喂狗；
 *      - 任一核心任务死锁、卡死或被高优先级饿死 → 停止喂狗 → 2.56s 内复位重启。
 *   4. 同步采样各任务栈高水位存入 g_stack_hwm，便于 Keil Watch 窗口实时观测余量。
 ******************************************************************/
#include "app_tasks.h"
#include "stm32f10x_iwdg.h"

#define IWDG_RELOAD_2P56S 800u /* 40kHz / 128 = 312.5Hz; 800 / 312.5 ≈ 2.56s */

/* 初始化并开启独立看门狗（main.c 已配置 DBGMCU_IWDG_STOP，断点调试不受影响） */
static void monitor_iwdg_start(void)
{
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_Prescaler_128);
    IWDG_SetReload(IWDG_RELOAD_2P56S);
    IWDG_ReloadCounter();
    IWDG_Enable();
}

/* 更新各任务栈剩余高水位（供仿真器 Watch 诊断） */
static void monitor_sample_stack_hwm(void)
{
#if (INCLUDE_uxTaskGetStackHighWaterMark == 1)
    if (g_task_rc_tx != NULL)
        g_stack_hwm.rc_tx = uxTaskGetStackHighWaterMark(g_task_rc_tx);
    if (g_task_input != NULL)
        g_stack_hwm.input = uxTaskGetStackHighWaterMark(g_task_input);
    if (g_task_storage != NULL)
        g_stack_hwm.storage = uxTaskGetStackHighWaterMark(g_task_storage);
    if (g_task_monitor != NULL)
        g_stack_hwm.monitor = uxTaskGetStackHighWaterMark(g_task_monitor);
    if (g_task_buzzer != NULL)
        g_stack_hwm.buzzer = uxTaskGetStackHighWaterMark(g_task_buzzer);
#endif
}

void Task_Monitor(void *param)
{
    TickType_t last_wake = xTaskGetTickCount();
    uint32_t heartbeat_bits;

    (void)param;

    /* 调度器就绪后才开启看门狗，避免上电自检重试期间误触发复位 */
    monitor_iwdg_start();

    for (;;)
    {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TASK_PERIOD_MONITOR_MS));

        /* 非阻塞取走并清空过去 500ms 内累计收到的所有任务心跳位 */
        heartbeat_bits = 0u;
        (void)xTaskNotifyWait(0u, 0xFFFFFFFFu, &heartbeat_bits, 0u);

        if ((heartbeat_bits & HEARTBEAT_ALL_MASK) == HEARTBEAT_ALL_MASK)
        {
            IWDG_ReloadCounter();
        }

        monitor_sample_stack_hwm();
    }
}

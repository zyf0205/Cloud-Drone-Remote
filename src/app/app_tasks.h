/*******************************************************************
 * FreeRTOS 应用层任务总览与任务间通信接口头文件
 *
 * 系统采用「4 业务任务 + 1 监控守护任务」解耦架构：
 *   1. Task_RC_Tx   (Prio 4, 10ms)  : 硬实时摇杆滤波 + 32B 组帧 + NRF 发送
 *   2. Task_Input   (Prio 3, 20ms)  : 8 路按键去抖状态机 + AUX/微调触发
 *   3. Task_Storage (Prio 2, 事件)  : 异步合并写入 AT24C02 EEPROM + 回读校验
 *   4. Task_Monitor (Prio 2, 500ms) : 汇总核心任务心跳位，全部存活才喂 IWDG
 *   5. Task_Buzzer  (Prio 1, 事件)  : 唯一拥有蜂鸣器 PA12，协调提示音与故障报警
 *
 * 内存预算（8KB Heap）：
 *   5 业务任务 + Idle 任务 + 2 个队列总计约消耗 ~4.0KB（< 50% 配额）。
 ******************************************************************/
#ifndef __APP_TASKS_H
#define __APP_TASKS_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "all_data.h"

/* ---------------- 任务优先级规划（configMAX_PRIORITIES = 8，Idle = 0） ---------------- */
#define TASK_PRIO_RC_TX   4u /* 最高业务优先级：保障 100Hz 无线链路确定性 */
#define TASK_PRIO_INPUT   3u /* 次高优先级：保障按键交互无感延迟 */
#define TASK_PRIO_STORAGE 2u /* 中优先级：异步写 EEPROM，不阻塞发包与按键 */
#define TASK_PRIO_MONITOR 2u /* 中优先级：周期检查心跳并喂狗 */
#define TASK_PRIO_BUZZER  1u /* 低优先级：后台声光提示与故障周期报警 */

/* ---------------- 任务栈深配置（单位：32 位字，1 字 = 4 字节） ---------------- */
#define TASK_STACK_RC_TX   160u /* 640B：含 32B 发送帧缓冲与浮点滤波栈帧 */
#define TASK_STACK_INPUT   128u /* 512B */
#define TASK_STACK_STORAGE 128u /* 512B */
#define TASK_STACK_MONITOR 128u /* 512B */
#define TASK_STACK_BUZZER  128u /* 512B */

/* ---------------- 任务运行周期（ms） ---------------- */
#define TASK_PERIOD_RC_TX_MS   10u  /* 100Hz 无线发包（与飞控端 10ms 轮询严格匹配） */
#define TASK_PERIOD_INPUT_MS   20u  /* 50Hz 按键扫描与 20ms 消抖节拍 */
#define TASK_PERIOD_MONITOR_MS 500u /* 2Hz 看门狗心跳汇总周期（IWDG 超时约 2.56s） */

/* ---------------- 核心任务心跳位掩码（通过 Task Notification 上报） ---------------- */
#define HEARTBEAT_BIT_RC_TX ((uint32_t)(1u << 0))
#define HEARTBEAT_BIT_INPUT ((uint32_t)(1u << 1))
#define HEARTBEAT_ALL_MASK  (HEARTBEAT_BIT_RC_TX | HEARTBEAT_BIT_INPUT)

/* ---------------- 蜂鸣器瞬态事件枚举 ---------------- */
typedef enum
{
    BEEP_EVT_NONE = 0,
    BEEP_EVT_KEY_CLICK,  /* 按键有效触发：单次短鸣（40ms） */
    BEEP_EVT_TRIM_LIMIT  /* 微调到达 ±250 限幅：双短促提示音 */
} buzzer_event_t;

/* 调试观测用：各任务栈高水位（剩余可用栈字数，数值越小越接近溢出） */
typedef struct
{
    UBaseType_t rc_tx;
    UBaseType_t input;
    UBaseType_t storage;
    UBaseType_t monitor;
    UBaseType_t buzzer;
} app_stack_hwm_t;

/* 全局任务与队列句柄（由 app_tasks.c 定义并初始化） */
extern TaskHandle_t g_task_rc_tx;
extern TaskHandle_t g_task_input;
extern TaskHandle_t g_task_storage;
extern TaskHandle_t g_task_monitor;
extern TaskHandle_t g_task_buzzer;

extern QueueHandle_t g_queue_storage; /* 深度 1，使用 xQueueOverwrite 合并微调写入 */
extern QueueHandle_t g_queue_buzzer;  /* 深度 8，缓存按键/限幅等瞬态音效事件 */

extern volatile app_stack_hwm_t g_stack_hwm;

/* 统一创建全部队列与 FreeRTOS 任务（返回 pdPASS=成功，pdFAIL=内存不足） */
extern BaseType_t App_TasksInit(void);

/* 任务间解耦通信 API */
extern void App_NotifyHeartbeat(uint32_t task_bit);
extern void App_PostTrimSave(const rc_trim_t *trim);
extern void App_PostBuzzerEvent(buzzer_event_t evt);

/* 各任务入口函数 */
extern void Task_RC_Tx(void *param);
extern void Task_Input(void *param);
extern void Task_Storage(void *param);
extern void Task_Buzzer(void *param);
extern void Task_Monitor(void *param);

#endif /* __APP_TASKS_H */

/*
 * FreeRTOSConfig.h - FreeRTOS V9.0.0 内核配置（遥控器端）
 *
 * 目标平台：STM32F103C8T6（Cortex-M3，HSI PLL 48MHz，64KB Flash / 20KB SRAM）
 * 编译工具：Keil MDK-ARM AC5 (armcc)，RVDS/ARM_CM3 移植层
 *
 * 系统时钟与中断规划：
 *  1. 主频 48MHz（HSI 8MHz / 2 × 12），system_stm32f10x.c 定义 SYSCLK_FREQ_48MHz，
 *     SystemCoreClock 与真实主频一致。
 *  2. SysTick（优先级 15）归 FreeRTOS 内核独占，提供 1kHz（1ms）系统节拍。
 *  3. TIM3（优先级 6，不调用 FreeRTOS API）在 APB1=24MHz（TIM3CLK=48MHz）下配置
 *     PSC=47、ARR=999，提供精确 1kHz（1ms）硬件毫秒时钟供开机初始化与超时判定使用。
 *  4. NVIC 优先级分组固定为 Group4（4 位抢占优先级，0 位响应优先级）。
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "stm32f10x.h"

/* ---------------- 基础调度配置 ---------------- */
#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     1 /* 空闲任务进入 __WFI() 降低功耗 */
#define configUSE_TICK_HOOK                     0
#define configUSE_TICKLESS_IDLE                 0
#define configUSE_16_BIT_TICKS                  0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configIDLE_SHOULD_YIELD                 1

#define configCPU_CLOCK_HZ                      ((unsigned long)SystemCoreClock)
#define configTICK_RATE_HZ                      ((TickType_t)1000)

#define configMAX_PRIORITIES                    (8)
#define configMINIMAL_STACK_SIZE                ((unsigned short)128)
#define configTOTAL_HEAP_SIZE                   ((size_t)(8 * 1024))
#define configMAX_TASK_NAME_LEN                 (12)

/* ---------------- 内核功能与调试监控 ---------------- */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           0
#define configUSE_TRACE_FACILITY                0
#define configUSE_APPLICATION_TASK_TAG          0
#define configUSE_TASK_NOTIFICATIONS            1 /* 用于核心任务向监控任务零开销上报心跳 */
#define configUSE_TIMERS                        0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 0
#define configCHECK_FOR_STACK_OVERFLOW          2 /* 栈底模式校验，覆盖度高于等级 1 */
#define configUSE_MALLOC_FAILED_HOOK            1 /* 开启堆分配失败钩子，确保钩子真正生效 */

/* ---------------- 中断优先级配置（Cortex-M3，4 位抢占优先级） ---------------- */
#define configPRIO_BITS                              4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

#define configKERNEL_INTERRUPT_PRIORITY              (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY         (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* ---------------- 移植层中断入口映射到启动文件向量表 ---------------- */
#define vPortSVCHandler                              SVC_Handler
#define xPortPendSVHandler                           PendSV_Handler
#define xPortSysTickHandler                          SysTick_Handler

/* ---------------- 启用的内核 API ---------------- */
#define INCLUDE_vTaskPrioritySet                     1
#define INCLUDE_uxTaskPriorityGet                    1
#define INCLUDE_vTaskDelete                          1
#define INCLUDE_vTaskSuspend                         1
#define INCLUDE_vTaskDelayUntil                      1
#define INCLUDE_vTaskDelay                           1
#define INCLUDE_xTaskGetSchedulerState               1
#define INCLUDE_uxTaskGetStackHighWaterMark          1

/* ---------------- 断言保护 ---------------- */
#define configASSERT(x)          \
    if ((x) == 0)                \
    {                            \
        taskDISABLE_INTERRUPTS(); \
        for (;;)                 \
            ;                    \
    }

#endif /* FREERTOS_CONFIG_H */

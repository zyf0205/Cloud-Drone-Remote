/*
 * FreeRTOSConfig.h - FreeRTOS V9.0.0 kernel configuration (遥控器端)
 * Target: STM32F103C8T6 (Cortex-M3, 48MHz HSI PLL, 20KB RAM)
 * Compiler: Keil MDK-ARM AC5 (armcc), RVDS/ARM_CM3 port
 *
 * 与飞控端（Cloud-Drone）的差异：
 *  - 遥控器使用内部 HSI（8M/2*12=48MHz），system_stm32f10x.c 已定义
 *    SYSCLK_FREQ_48MHz，SystemCoreClock 为真实主频
 *  - 系统 1ms 毫秒时钟由 TIM3 提供（PSC=23, ARR=999，APB1=24MHz），
 *    SysTick 归 FreeRTOS 所有
 *  - NVIC 优先级分组 Group4；TIM3=6（毫秒时钟，不调 API）、
 *    SysTick/PendSV=15（内核）
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "stm32f10x.h"

/* ---------------- 基础配置 ---------------- */
#define configUSE_PREEMPTION                       1
#define configUSE_IDLE_HOOK                        0
#define configUSE_TICK_HOOK                        0
#define configUSE_TICKLESS_IDLE                    0
#define configUSE_16_BIT_TICKS                     0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION    1
#define configIDLE_SHOULD_YIELD                    1

#define configCPU_CLOCK_HZ                         ( ( unsigned long ) SystemCoreClock )
#define configTICK_RATE_HZ                         ( ( TickType_t ) 1000 )

#define configMAX_PRIORITIES                       ( 8 )
#define configMINIMAL_STACK_SIZE                   ( ( unsigned short ) 128 )
#define configTOTAL_HEAP_SIZE                      ( ( size_t ) ( 8 * 1024 ) )
#define configMAX_TASK_NAME_LEN                    ( 12 )

/* ---------------- 内核功能 ---------------- */
#define configUSE_MUTEXES                          1
#define configUSE_RECURSIVE_MUTEXES                0
#define configUSE_COUNTING_SEMAPHORES              0
#define configUSE_TRACE_FACILITY                   0
#define configUSE_APPLICATION_TASK_TAG             0
#define configUSE_TASK_NOTIFICATIONS               1
#define configUSE_TIMERS                           0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS    0
#define configCHECK_FOR_STACK_OVERFLOW             1

/* ---------------- 中断优先级（Cortex-M3, 4位优先级） ---------------- */
#define configPRIO_BITS                            4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY     15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

#define configKERNEL_INTERRUPT_PRIORITY            ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY       ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )

/* ---------------- 移植层中断处理函数改名到启动文件向量表 ---------------- */
#define vPortSVCHandler            SVC_Handler
#define xPortPendSVHandler         PendSV_Handler
#define xPortSysTickHandler        SysTick_Handler

/* ---------------- API 裁剪 ---------------- */
#define INCLUDE_vTaskPrioritySet                  1
#define INCLUDE_uxTaskPriorityGet                 1
#define INCLUDE_vTaskDelete                       1
#define INCLUDE_vTaskSuspend                      1
#define INCLUDE_vTaskDelayUntil                   1
#define INCLUDE_vTaskDelay                        1
#define INCLUDE_xTaskGetSchedulerState            1
#define INCLUDE_uxTaskGetStackHighWaterMark       1

/* ---------------- 错误处理 ---------------- */
#define configASSERT( x )                          if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }

#endif /* FREERTOS_CONFIG_H */

/*******************************************************************
 * 全局共享数据类型与系统故障标志定义
 *
 * 注意：_st_Remote 通道字段顺序（roll/pitch/thr/yaw/AUX1~4）与
 * 无线 32 字节帧格式保持一致，切勿随意调整。
 ******************************************************************/
#ifndef _ALL_USER_DATA_H_
#define _ALL_USER_DATA_H_

#include <stdint.h>

/* 硬件毫秒计数器（由 TIM3 1kHz 更新中断累加，调度器启动前后均可用） */
extern volatile uint32_t SysTick_count;

/* 遥控 8 通道数据（量程 1000~2000，中位 1500） */
typedef struct
{
    uint16_t roll;  /* 横滚通道 */
    uint16_t pitch; /* 俯仰通道 */
    uint16_t thr;   /* 油门通道（缩放系数 0.20，配合飞控定高死区） */
    uint16_t yaw;   /* 偏航通道 */
    uint16_t AUX1;  /* 定高开关（1000 关 / 2000 开） */
    uint16_t AUX2;  /* 光流定点开关（1000 关 / 2000 开） */
    uint16_t AUX3;  /* 陀螺仪校准触发（飞控端在未解锁+最低油门时响应） */
    uint16_t AUX4;  /* 预留辅助通道 */
} _st_Remote;

/* 摇杆中位微调快照（单位：通道值偏移，范围 [-250, +250]，步进 5） */
typedef struct
{
    int16_t roll;
    int16_t pitch;
} rc_trim_t;

/* 全局最新遥控通道快照（供调试观测及协议发送参考） */
extern volatile _st_Remote Remote;

/*
 * 系统故障标志位（可由启动自检或运行期检测置位，由 Task_Buzzer 统一报警）
 *  - FAULT_NRF   ：NRF 模块不在位或持续通信故障，停止发送并每秒长鸣 0.4s
 *  - FAULT_EEPROM：EEPROM 异常，微调强制清零且禁用写入，每秒两短鸣（仍可飞行）
 */
#define FAULT_NRF    ((uint8_t)0x01u)
#define FAULT_EEPROM ((uint8_t)0x02u)

extern volatile uint8_t g_boot_fault;

#endif /* _ALL_USER_DATA_H_ */

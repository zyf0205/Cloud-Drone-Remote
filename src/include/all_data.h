#ifndef _ALL_USER_DATA_H_
#define _ALL_USER_DATA_H_

#include <stdint.h>

extern volatile uint32_t SysTick_count;

/* 遥控 4 通道 + AUX1~4（大端发送顺序依赖该结构体字段顺序） */
typedef volatile struct
{
    uint16_t roll;
    uint16_t pitch;
    uint16_t thr;
    uint16_t yaw;
    uint16_t AUX1;
    uint16_t AUX2;
    uint16_t AUX3;
    uint16_t AUX4;
} _st_Remote;

extern _st_Remote Remote;

/* 启动自检故障标志(init.c 置位,task_rc.c 蜂鸣报警) */
#define FAULT_NRF     0x01  /* NRF 模块不在位:跳过发送并长鸣报警,禁止使用 */
#define FAULT_EEPROM  0x02  /* EEPROM 异常:微调不可用(清零处理),两短鸣提示 */
extern volatile uint8_t g_boot_fault;

#endif

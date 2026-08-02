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

#endif

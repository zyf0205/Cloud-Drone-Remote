/*******************************************************************
 * 遥控器 8 路按键板级驱动与单键去抖边沿状态机头文件
 *
 * 引脚定义（全部为输入上拉 GPIO_Mode_IPU，未按=高电平，按下=低电平）：
 *   AUX1 = PA15（定高开关）    AUX2 = PB11（光流定点开关）
 *   AUX3 = PC14（陀螺校准）    AUX4 = PC15（预留）
 *   左微调 = PA6（Roll -5）    右微调 = PA7（Roll +5）
 *   前微调 = PB0（Pitch -5）   后微调 = PB1（Pitch +5）
 ******************************************************************/
#ifndef __KEY_H
#define __KEY_H

#include <stdint.h>

/* 8 路按键位掩码（bit=1 表示该键当前被按下） */
#define KEY_MASK_AUX1       ((uint8_t)(1u << 0))
#define KEY_MASK_AUX2       ((uint8_t)(1u << 1))
#define KEY_MASK_AUX3       ((uint8_t)(1u << 2))
#define KEY_MASK_AUX4       ((uint8_t)(1u << 3))
#define KEY_MASK_TRIM_LEFT  ((uint8_t)(1u << 4))
#define KEY_MASK_TRIM_RIGHT ((uint8_t)(1u << 5))
#define KEY_MASK_TRIM_FRONT ((uint8_t)(1u << 6))
#define KEY_MASK_TRIM_BACK  ((uint8_t)(1u << 7))

/* 按键去抖后输出的边沿事件（单次触发，松手后解锁下一次） */
typedef enum
{
    KEY_EVT_NONE = 0,
    KEY_EVT_AUX1,
    KEY_EVT_AUX2,
    KEY_EVT_AUX3,
    KEY_EVT_AUX4,
    KEY_EVT_TRIM_LEFT,
    KEY_EVT_TRIM_RIGHT,
    KEY_EVT_TRIM_FRONT,
    KEY_EVT_TRIM_BACK
} key_event_t;

/* 按键状态机内部状态（开放给模块实例与单元测试） */
typedef enum
{
    KEY_FSM_IDLE = 0,     /* 空闲：等待任意按键按下 */
    KEY_FSM_DEBOUNCE,     /* 消抖：连续一拍确认有效按下后触发单次事件 */
    KEY_FSM_WAIT_RELEASE  /* 锁定：等待全部 8 键松开后才回到 IDLE，防连发 */
} key_fsm_state_t;

typedef struct
{
    key_fsm_state_t state;
    uint8_t candidate_mask;
} key_fsm_t;

extern void Key_Init(void);
extern uint8_t Key_ReadPressedMask(void);
extern void Key_FsmInit(key_fsm_t *fsm);
extern key_event_t Key_FsmStep(key_fsm_t *fsm, uint8_t pressed_mask);
extern key_event_t Key_Scan(void);

#endif /* __KEY_H */

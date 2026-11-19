/*******************************************************************
 * 遥控通信协议与摇杆滤波纯算法模块头文件
 *
 * 设计特点：
 *   本模块不依赖任何 STM32 外设或 FreeRTOS 接口，只处理纯数学与字节流，
 *   既便于阅读协议规范，也可直接在主机端用标准 C 编译器运行单元测试。
 *
 * 与飞控端（Cloud-Drone）对接契约（严禁单方面改动）：
 *   帧长 32 字节：[0xAA, 0xAF, 0x03, 0x1B] + 8 通道大端(4..19)
 *                 + 保留零(20..30) + 前 31 字节求和校验(31)
 ******************************************************************/
#ifndef __RC_PROTOCOL_H
#define __RC_PROTOCOL_H

#include <stdint.h>
#include "all_data.h"

/* ---------------- 通道量程与映射参数 ---------------- */
#define RC_CH_MIN       1000u /* 通道输出下限 */
#define RC_CH_MAX       2000u /* 通道输出上限 */
#define RC_STICK_SCALE  0.25f /* Roll/Pitch/Yaw 映射系数：ADC(0~4095) → 1000~2023 */
#define RC_THR_SCALE    0.20f /* 油门缩放系数，满杆约 1819。
                                 ⚠ 严禁改为 0.25：飞控端定高死区上限对应通道值 1750，
                                 0.20 时需 92% 行程才触发上升，0.25 时 73% 即触发，
                                 会导致常规悬停杆位被误判为持续上升（实测回退注记） */
#define RC_LPF_ALPHA    0.25f /* 一阶低通滤波系数：0.25 × 新值 + 0.75 × 上次值 */

/* ---------------- 微调参数约束 ---------------- */
#define RC_TRIM_STEP    5     /* 单次微调步进 */
#define RC_TRIM_LIMIT   250   /* 微调合法上下界 [-250, +250] */

/* ---------------- 32 字节无线帧格式常量 ---------------- */
#define RC_FRAME_LEN       32u   /* 固定帧长（与 NRF TX_PLOAD_WIDTH 一致） */
#define RC_FRAME_HEAD0     0xAAu /* 帧头字节 0 */
#define RC_FRAME_HEAD1     0xAFu /* 帧头字节 1 */
#define RC_FRAME_FUNC      0x03u /* 保留/功能字节 2 */
#define RC_FRAME_PAYLOAD   0x1Bu /* 保留/长度字节 3 (32 - 5 = 27) */
#define RC_CHECKSUM_OFFSET 31u   /* 校验和字节偏移 */

/* 一阶低通滤波器状态（带首帧初始化标记，避免上电第一帧从 0 爬坡） */
typedef struct
{
    uint16_t last_val;
    uint8_t initialized;
} rc_lpf_t;

extern uint16_t RC_ClampChannel(int32_t value);
extern void RC_LpfReset(rc_lpf_t *lpf);
extern uint16_t RC_StickFilterStep(uint16_t adc_raw, float scale, int16_t trim, rc_lpf_t *lpf);
extern uint8_t RC_TrimIsValid(int16_t trim);
extern uint8_t RC_TrimAdjust(int16_t *trim, int16_t delta);
extern void RC_FramePack(uint8_t frame[RC_FRAME_LEN], const _st_Remote *ch);

#endif /* __RC_PROTOCOL_H */

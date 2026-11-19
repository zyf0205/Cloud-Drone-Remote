/*******************************************************************
 * 遥控通信协议与摇杆滤波纯算法实现
 *
 * 包含：
 *   1. 通道值钳位 [1000, 2000]（有符号中间值，杜绝无符号下溢）
 *   2. 摇杆 ADC 线性映射 + 微调叠加 + 一阶低通滤波
 *   3. 微调值合法性检查（拦截新片 EEPROM 全 0xFF=-1 等脏数据）与限幅步进
 *   4. 32 字节大端帧组包与前 31 字节求和校验
 ******************************************************************/
#include "rc_protocol.h"

/* 写入 16 位大端字节序 */
static void frame_put_be16(uint8_t *buf, uint8_t pos, uint16_t v)
{
    buf[pos]      = (uint8_t)(v >> 8);
    buf[pos + 1u] = (uint8_t)(v & 0xFFu);
}

/* 将有符号计算结果钳位到合法遥控量程 [RC_CH_MIN, RC_CH_MAX] */
uint16_t RC_ClampChannel(int32_t value)
{
    if (value < (int32_t)RC_CH_MIN)
        return RC_CH_MIN;
    if (value > (int32_t)RC_CH_MAX)
        return RC_CH_MAX;
    return (uint16_t)value;
}

/* 复位通道低通滤波器状态 */
void RC_LpfReset(rc_lpf_t *lpf)
{
    lpf->last_val    = RC_CH_MIN;
    lpf->initialized = 0u;
}

/*
 * 单通道摇杆更新：ADC(0~4095) → 线性映射 + 微调 → 一阶低通 → 钳位
 * 说明：首帧直接初始化滤波器状态为当前真实映射值，避免上电瞬间出现从 0 爬坡的瞬态毛刺。
 */
uint16_t RC_StickFilterStep(uint16_t adc_raw, float scale, int16_t trim, rc_lpf_t *lpf)
{
    int32_t mapped = (int32_t)RC_CH_MIN + (int32_t)(scale * (float)adc_raw) + (int32_t)trim;
    uint16_t raw_ch = RC_ClampChannel(mapped);
    uint16_t filtered;

    if (lpf->initialized == 0u)
    {
        filtered         = raw_ch;
        lpf->initialized = 1u;
    }
    else
    {
        float y  = (float)raw_ch * RC_LPF_ALPHA + (1.0f - RC_LPF_ALPHA) * (float)lpf->last_val;
        filtered = RC_ClampChannel((int32_t)y);
    }

    lpf->last_val = filtered;
    return filtered;
}

/*
 * 校验从 EEPROM 读回的微调值是否合法：
 *   必须位于 [-250, +250] 且为 RC_TRIM_STEP(5) 的整数倍。
 *   全新未写入的 AT24C02 全为 0xFF（解读为 int16_t 即 -1），因不是 5 的倍数会被自动拦下归零，
 *   同时 100% 兼容历史固件按步进 5 写入的所有合法微调值。
 */
uint8_t RC_TrimIsValid(int16_t trim)
{
    if (trim < -RC_TRIM_LIMIT || trim > RC_TRIM_LIMIT)
        return 0u;
    if ((trim % RC_TRIM_STEP) != 0)
        return 0u;
    return 1u;
}

/*
 * 按步进调整微调并限幅在 [-RC_TRIM_LIMIT, +RC_TRIM_LIMIT]：
 *   返回 1 表示微调值实际发生了改变；返回 0 表示已处于限幅边界。
 */
uint8_t RC_TrimAdjust(int16_t *trim, int16_t delta)
{
    int32_t next = (int32_t)(*trim) + (int32_t)delta;

    if (next < -RC_TRIM_LIMIT)
        next = -RC_TRIM_LIMIT;
    else if (next > RC_TRIM_LIMIT)
        next = RC_TRIM_LIMIT;

    if ((int16_t)next == *trim)
        return 0u;

    *trim = (int16_t)next;
    return 1u;
}

/*
 * 打包 32 字节遥控无线发送帧（与飞控端 RC_Analy 严格一致）：
 *   [0..3]   : 0xAA, 0xAF, 0x03, 0x1B
 *   [4..19]  : roll, pitch, thr, yaw, AUX1, AUX2, AUX3, AUX4（各 2 字节大端）
 *   [20..30] : 保留字节固定填 0x00
 *   [31]     : 前 31 字节无符号求和低 8 位
 */
void RC_FramePack(uint8_t frame[RC_FRAME_LEN], const _st_Remote *ch)
{
    uint8_t i;
    uint16_t sum = 0u;

    frame[0] = RC_FRAME_HEAD0;
    frame[1] = RC_FRAME_HEAD1;
    frame[2] = RC_FRAME_FUNC;
    frame[3] = RC_FRAME_PAYLOAD;

    frame_put_be16(frame, 4u, ch->roll);
    frame_put_be16(frame, 6u, ch->pitch);
    frame_put_be16(frame, 8u, ch->thr);
    frame_put_be16(frame, 10u, ch->yaw);
    frame_put_be16(frame, 12u, ch->AUX1);
    frame_put_be16(frame, 14u, ch->AUX2);
    frame_put_be16(frame, 16u, ch->AUX3);
    frame_put_be16(frame, 18u, ch->AUX4);

    for (i = 20u; i < RC_CHECKSUM_OFFSET; i++)
    {
        frame[i] = 0u;
    }

    for (i = 0u; i < RC_CHECKSUM_OFFSET; i++)
    {
        sum = (uint16_t)(sum + frame[i]);
    }
    frame[RC_CHECKSUM_OFFSET] = (uint8_t)(sum & 0xFFu);
}

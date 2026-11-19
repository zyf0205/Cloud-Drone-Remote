/*******************************************************************
 * 遥控状态中心实现
 *
 * 职责划分（高内聚、低耦合）：
 *   - 本模块仅负责管理遥控 8 通道与微调偏移的线程安全状态，以及协调
 *     rc_protocol 纯算法生成发送帧；
 *   - 不再直接操作 NRF 发送、EEPROM 写入、蜂鸣器引脚或按键硬件寄存器，
 *     这些慢速/并发硬件访问已分别下沉到各自专属的 FreeRTOS 任务中。
 ******************************************************************/
#include "FreeRTOS.h"
#include "task.h"
#include "remote.h"
#include "eeprom.h"
#include "key.h"

/* 全局最新通道值（保留原符号名，方便调试观测） */
volatile _st_Remote Remote = {
    RC_CH_MIN, RC_CH_MIN, RC_CH_MIN, RC_CH_MIN,
    RC_CH_MIN, RC_CH_MIN, RC_CH_MIN, RC_CH_MIN
};

/* 摇杆中位微调偏移（受临界区保护） */
static rc_trim_t s_trim = {0, 0};

/* 4 路摇杆独立一阶低通滤波器（仅由 Task_RC_Tx 访问，无需加锁） */
static rc_lpf_t s_lpf_roll;
static rc_lpf_t s_lpf_pitch;
static rc_lpf_t s_lpf_thr;
static rc_lpf_t s_lpf_yaw;

/* 从 EEPROM 安全加载一个微调值（I2C 异常或数据越界均安全归零） */
static int16_t rc_load_trim_axis(uint8_t eeprom_addr)
{
    int16_t val = 0;

    if (EEPROM_ReadI16(eeprom_addr, &val) != 0u)
    {
        g_boot_fault |= FAULT_EEPROM;
        return 0;
    }

    if (!RC_TrimIsValid(val))
        return 0;

    return val;
}

/*
 * 遥控状态初始化（调度器启动前由 ALL_Init 调用）：
 *   1. 复位 4 路低通滤波器与 8 通道默认初值；
 *   2. 若 EEPROM 自检正常则加载并校验 Roll/Pitch 微调，否则强制归零；
 *   3. 初始化 8 路按键硬件与去抖状态机。
 */
void RC_INIT(void)
{
    Remote.roll  = RC_CH_MIN;
    Remote.pitch = RC_CH_MIN;
    Remote.thr   = RC_CH_MIN;
    Remote.yaw   = RC_CH_MIN;
    Remote.AUX1  = RC_CH_MIN;
    Remote.AUX2  = RC_CH_MIN;
    Remote.AUX3  = RC_CH_MIN;
    Remote.AUX4  = RC_CH_MIN;

    RC_LpfReset(&s_lpf_roll);
    RC_LpfReset(&s_lpf_pitch);
    RC_LpfReset(&s_lpf_thr);
    RC_LpfReset(&s_lpf_yaw);

    if (g_boot_fault & FAULT_EEPROM)
    {
        s_trim.roll  = 0;
        s_trim.pitch = 0;
    }
    else
    {
        s_trim.roll  = rc_load_trim_axis(ROLL_ADDR_OFFSET);
        s_trim.pitch = rc_load_trim_axis(PITCH_ADDR_OFFSET);
    }

    Key_Init();
}

/*
 * 根据当前 ADC 快照更新 4 路摇杆并打包 32 字节无线发送帧（由 Task_RC_Tx 每 10ms 调用）
 */
void RC_UpdateSticksAndBuildFrame(const uint16_t adc_raw[ADC_CH_COUNT], uint8_t tx_frame[RC_FRAME_LEN])
{
    rc_trim_t trim_snap;
    _st_Remote ch_snap;

    /* 临界区仅拷贝 12 字节快照，耗时 < 2us，几乎不影响中断响应 */
    taskENTER_CRITICAL();
    trim_snap     = s_trim;
    ch_snap.AUX1  = Remote.AUX1;
    ch_snap.AUX2  = Remote.AUX2;
    ch_snap.AUX3  = Remote.AUX3;
    ch_snap.AUX4  = Remote.AUX4;
    taskEXIT_CRITICAL();

    /* 4 路摇杆映射 + 微调 + 一阶低通 + 钳位 */
    ch_snap.thr   = RC_StickFilterStep(adc_raw[ADC_CH_THR], RC_THR_SCALE, 0, &s_lpf_thr);
    ch_snap.pitch = RC_StickFilterStep(adc_raw[ADC_CH_PITCH], RC_STICK_SCALE, trim_snap.pitch, &s_lpf_pitch);
    ch_snap.yaw   = RC_StickFilterStep(adc_raw[ADC_CH_YAW], RC_STICK_SCALE, 0, &s_lpf_yaw);
    ch_snap.roll  = RC_StickFilterStep(adc_raw[ADC_CH_ROLL], RC_STICK_SCALE, trim_snap.roll, &s_lpf_roll);

    /* 更新全局观测快照 */
    taskENTER_CRITICAL();
    Remote.roll  = ch_snap.roll;
    Remote.pitch = ch_snap.pitch;
    Remote.thr   = ch_snap.thr;
    Remote.yaw   = ch_snap.yaw;
    taskEXIT_CRITICAL();

    /* 组装 32 字节大端无线发送帧 */
    RC_FramePack(tx_frame, &ch_snap);
}

/* 线程安全翻转指定 AUX 开关（1000 ↔ 2000），返回翻转后的新值（由 Task_Input 调用） */
uint16_t RC_ToggleAux(uint8_t aux_idx)
{
    uint16_t next_val = RC_CH_MIN;

    taskENTER_CRITICAL();
    switch (aux_idx)
    {
        case RC_AUX_IDX_1:
            Remote.AUX1 = (Remote.AUX1 == RC_CH_MIN) ? RC_CH_MAX : RC_CH_MIN;
            next_val    = Remote.AUX1;
            break;
        case RC_AUX_IDX_2:
            Remote.AUX2 = (Remote.AUX2 == RC_CH_MIN) ? RC_CH_MAX : RC_CH_MIN;
            next_val    = Remote.AUX2;
            break;
        case RC_AUX_IDX_3:
            Remote.AUX3 = (Remote.AUX3 == RC_CH_MIN) ? RC_CH_MAX : RC_CH_MIN;
            next_val    = Remote.AUX3;
            break;
        case RC_AUX_IDX_4:
            Remote.AUX4 = (Remote.AUX4 == RC_CH_MIN) ? RC_CH_MAX : RC_CH_MIN;
            next_val    = Remote.AUX4;
            break;
        default:
            break;
    }
    taskEXIT_CRITICAL();

    return next_val;
}

/*
 * 线程安全调整 Roll/Pitch 微调偏移（由 Task_Input 调用）：
 *   is_pitch     : 0=调整 Roll，非 0=调整 Pitch
 *   delta        : 步进增量（+RC_TRIM_STEP 或 -RC_TRIM_STEP）
 *   out_snapshot : 输出调整后的最新微调快照（用于投递给 Task_Storage 异步落盘）
 *   返回         : 1=数值发生变化，0=已到达 ±250 限幅边界
 */
uint8_t RC_AdjustTrim(uint8_t is_pitch, int16_t delta, rc_trim_t *out_snapshot)
{
    uint8_t changed;

    taskENTER_CRITICAL();
    if (is_pitch)
        changed = RC_TrimAdjust(&s_trim.pitch, delta);
    else
        changed = RC_TrimAdjust(&s_trim.roll, delta);

    if (out_snapshot != NULL)
        *out_snapshot = s_trim;
    taskEXIT_CRITICAL();

    return changed;
}

/* 线程安全读取当前微调快照（供 Task_Storage 初始化已落盘基准值） */
void RC_GetTrimSnapshot(rc_trim_t *out_snapshot)
{
    if (out_snapshot == NULL)
        return;

    taskENTER_CRITICAL();
    *out_snapshot = s_trim;
    taskEXIT_CRITICAL();
}

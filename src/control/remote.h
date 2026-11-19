/*******************************************************************
 * 遥控状态中心头文件（跨任务共享通道与微调的线程安全入口）
 *
 * 线程安全边界：
 *   - Task_RC_Tx 调用 RC_UpdateSticksAndBuildFrame() 读取微调与 AUX 状态、
 *     更新 4 路摇杆并生成 32 字节待发送帧。
 *   - Task_Input 调用 RC_ToggleAux() / RC_AdjustTrim() 修改 AUX 与微调。
 *   - 内部通过极短的 FreeRTOS 临界区保护共享字段，避免多锁死锁与优先级反转。
 ******************************************************************/
#ifndef _REMOTE_H
#define _REMOTE_H

#include <stdint.h>
#include "all_data.h"
#include "adc.h"
#include "rc_protocol.h"

/* AUX 通道索引（0~3 分别对应 AUX1~AUX4） */
#define RC_AUX_IDX_1 0u
#define RC_AUX_IDX_2 1u
#define RC_AUX_IDX_3 2u
#define RC_AUX_IDX_4 3u

extern void RC_INIT(void);
extern void RC_UpdateSticksAndBuildFrame(const uint16_t adc_raw[ADC_CH_COUNT], uint8_t tx_frame[RC_FRAME_LEN]);
extern uint16_t RC_ToggleAux(uint8_t aux_idx);
extern uint8_t RC_AdjustTrim(uint8_t is_pitch, int16_t delta, rc_trim_t *out_snapshot);
extern void RC_GetTrimSnapshot(rc_trim_t *out_snapshot);

#endif /* _REMOTE_H */

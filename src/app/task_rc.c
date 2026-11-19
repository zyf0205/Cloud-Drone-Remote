/*******************************************************************
 * Task_RC_Tx — 核心无线控制与发送任务（优先级 4，10ms 严格周期）
 *
 * 单一硬件所有权：唯一允许操作 NRF24L01+ 模块的任务。
 *
 * 每 10ms（100Hz）串行执行纯硬实时主链路：
 *   1. 读取 ADC1+DMA 后台循环刷新的 4 路摇杆快照；
 *   2. 合并微调与 AUX 状态 → 一阶低通滤波 → 钳位 [1000,2000] → 组 32B 大端帧；
 *   3. 若 NRF 正常则无线发送；遇到偶发超时自动清 FIFO，连续超时则尝试软恢复，
 *      若硬件已脱焊/掉电则置位 FAULT_NRF 交由 Task_Buzzer 报警；
 *   4. 向 Task_Monitor 上报心跳位 HEARTBEAT_BIT_RC_TX。
 *
 * 实时性保障：
 *   本任务内无任何 EEPROM 写延时、按键消抖或蜂鸣器控制，发包周期抖动极低。
 ******************************************************************/
#include "app_tasks.h"
#include "adc.h"
#include "remote.h"
#include "nrf24l01.h"

/* 连续发送超时达到该次数（20 × 10ms = 200ms）后触发 NRF 自检与恢复 */
#define NRF_CONSEC_TIMEOUT_LIMIT 20u

void Task_RC_Tx(void *param)
{
    TickType_t last_wake = xTaskGetTickCount();
    uint16_t adc_snap[ADC_CH_COUNT];
    uint8_t tx_frame[RC_FRAME_LEN];
    uint8_t timeout_cnt = 0u;

    (void)param;

    for (;;)
    {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TASK_PERIOD_RC_TX_MS));

        /* 1. 采集 4 路摇杆快照并生成 32 字节标准协议帧 */
        ADC1_ReadSnapshot(adc_snap);
        RC_UpdateSticksAndBuildFrame(adc_snap, tx_frame);

        /* 2. NRF 无线发送与运行期超时自愈 */
        if ((g_boot_fault & FAULT_NRF) == 0u)
        {
            u8 tx_ret = NRF24L01_TxPacket(tx_frame);
            if (tx_ret == TX_OK)
            {
                timeout_cnt = 0u;
            }
            else if (tx_ret == NRF_TX_TIMEOUT)
            {
                timeout_cnt++;
                if (timeout_cnt >= NRF_CONSEC_TIMEOUT_LIMIT)
                {
                    timeout_cnt = 0u;
                    if (NRF24L01_Check() == 0u)
                    {
                        TX_Mode(); /* 模块仍在位，重新下发 TX 配置尝试自愈 */
                    }
                    else
                    {
                        App_SetBootFault(FAULT_NRF); /* 模块已离线，停发并触发故障报警 */
                    }
                }
            }
        }

        /* 3. 上报本周期存活心跳 */
        App_NotifyHeartbeat(HEARTBEAT_BIT_RC_TX);
    }
}

/*******************************************************************
 * Task_Storage — EEPROM 异步持久化任务（优先级 2，事件阻塞驱动）
 *
 * 单一硬件所有权：调度器启动后唯一访问软件 I2C 与 AT24C02 的任务。
 *
 * 设计亮点：
 *   1. 零空转功耗：无微调写入请求时永久阻塞在 xQueueReceive()，不占 CPU；
 *   2. 写入合并：上游通过 xQueueOverwrite() 投递最新快照，连续快按微调键
 *      只落盘最终值，大幅减少 EEPROM 擦写次数；
 *   3. 脏检查 + 非忙等延时：仅写入数值实际发生变化的轴，写周期等待改用
 *      vTaskDelay(10ms) 让出 CPU，彻底消除对 100Hz 无线发包的抖动；
 *   4. 写后回读校验：验证写入结果，若芯片异常则置位 FAULT_EEPROM 报警。
 ******************************************************************/
#include "app_tasks.h"
#include "eeprom.h"
#include "remote.h"

/* 向指定地址写入微调值并等待写周期后回读校验（0=成功，1=失败） */
static uint8_t storage_save_axis(uint8_t addr, int16_t val)
{
    int16_t verify = 0;

    if (EEPROM_WriteI16Raw(addr, val) != 0u)
        return 1u;

    /* 让出 CPU 等待 AT24C02 内部页写周期完成（不占用 CPU，允许低功耗休眠） */
    vTaskDelay(pdMS_TO_TICKS(EEPROM_WRITE_WAIT_MS));

    if (EEPROM_ReadI16(addr, &verify) != 0u)
        return 1u;

    return (verify == val) ? 0u : 1u;
}

void Task_Storage(void *param)
{
    rc_trim_t saved_trim;
    rc_trim_t req_trim;

    (void)param;

    /* 以开机加载后的微调值作为已落盘基线，避免重复写未变动的轴 */
    RC_GetTrimSnapshot(&saved_trim);

    for (;;)
    {
        if (xQueueReceive(g_queue_storage, &req_trim, portMAX_DELAY) != pdPASS)
            continue;

        /* EEPROM 已标记故障时直接跳过，避免反复操作失效芯片 */
        if (g_boot_fault & FAULT_EEPROM)
            continue;

        if (req_trim.roll != saved_trim.roll)
        {
            if (storage_save_axis(ROLL_ADDR_OFFSET, req_trim.roll) == 0u)
                saved_trim.roll = req_trim.roll;
            else
                App_SetBootFault(FAULT_EEPROM);
        }

        if ((g_boot_fault & FAULT_EEPROM) == 0u && req_trim.pitch != saved_trim.pitch)
        {
            if (storage_save_axis(PITCH_ADDR_OFFSET, req_trim.pitch) == 0u)
                saved_trim.pitch = req_trim.pitch;
            else
                App_SetBootFault(FAULT_EEPROM);
        }
    }
}

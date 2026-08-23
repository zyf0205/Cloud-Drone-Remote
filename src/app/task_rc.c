#include "all_define.h"
#include "app_tasks.h"
#include "remote.h"
#include "stm32f10x_iwdg.h"

/*
 * 启动自检故障报警（非阻塞，每 10ms 调用，1s 一个循环）：
 * NRF 故障 = 长鸣 0.4s（无法发送，禁止使用）
 * 仅 EEPROM 故障 = 每秒两短鸣（微调不可用，仍可飞行）
 * 与按键提示音偶发交叠时以本函数为准，不影响故障提醒
 */
#define FAULT_BEEP_PERIOD_MS  1000u /* 报警循环周期 */
#define NRF_BEEP_ON_MS        400u  /* NRF 故障长鸣时长 */
#define EEPROM_BEEP1_ON_MS    100u  /* EEPROM 故障第一短鸣时长 */
#define EEPROM_BEEP2_START_MS 200u  /* EEPROM 故障第二短鸣起点 */
#define EEPROM_BEEP2_ON_MS    100u  /* EEPROM 故障第二短鸣时长 */

static void fault_beep(void)
{
    uint32_t t = SysTick_count % FAULT_BEEP_PERIOD_MS;
    uint8_t on = 0;

    if (g_boot_fault & FAULT_NRF)
        on = (t < NRF_BEEP_ON_MS);
    else if (g_boot_fault & FAULT_EEPROM)
        on = (t < EEPROM_BEEP1_ON_MS) || (t >= EEPROM_BEEP2_START_MS && t < EEPROM_BEEP2_START_MS + EEPROM_BEEP2_ON_MS);

    if (on)
        BEEP_H;
    else
        BEEP_L;
}

/*
 * 遥控任务：10ms 周期（与旧版裸机 SysTick 分频调度一致）。
 * 采集 ADC 摇杆 → 滤波钳位 → 按键扫描 → NRF24L01 发送 32 字节帧。
 * 含 IWDG 看门狗：任务启动后才开狗,此后每周期喂一次;
 * 任何环节挂死 → 约 2.6s 内复位重启恢复发送,
 * 远快于飞控端 5s 失联门限,空中链路可自动恢复。
 */
void RCTask(void *param)
{
    TickType_t lastWakeTime = xTaskGetTickCount();

    /* IWDG 独立看门狗:LSI 标称 40kHz,分频 128、重载 800 → 超时约 2.56s */
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_Prescaler_128);
    IWDG_SetReload(800);
    IWDG_ReloadCounter();
    IWDG_Enable();

    for (;;)
    {
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(10));
        IWDG_ReloadCounter(); /* 喂狗 */
        fault_beep();         /* 启动自检故障周期蜂鸣提醒 */
        RC_Analy();
    }
}

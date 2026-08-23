/*******************************************************************
 * 遥控器核心逻辑
 * 摇杆 ADC 采集 → 滤波钳位 → 按键/微调扫描 → NRF24L01 组帧发送
 *
 * 按键引脚（低电平有效，输入上拉）：
 *   AUX1 = PA15  AUX2 = PB11  AUX3 = PC14  AUX4 = PC15
 *   左微调 = PA6  右微调 = PA7  前微调 = PB0  后微调 = PB1
 ******************************************************************/
#include "stm32f10x.h"
#include "remote.h"
#include "adc.h"
#include "sys.h"
#include "nrf24l01.h"
#include "eeprom.h"

__align(4) _st_Remote Remote = {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000};

/* 摇杆中位微调值（EEPROM 持久化） */
static struct
{
    int16_t roll;
    int16_t pitch;
    int16_t thr;
    int16_t yaw;
} offset = {0};

#define KEY_AUX1  GPIO_Pin_15 /* GPIOA */
#define KEY_AUX2  GPIO_Pin_11 /* GPIOB */
#define KEY_AUX3  GPIO_Pin_14 /* GPIOC */
#define KEY_AUX4  GPIO_Pin_15 /* GPIOC */
#define KEY_FRONT GPIO_Pin_0  /* GPIOB */
#define KEY_BACK  GPIO_Pin_1  /* GPIOB */
#define KEY_LEFT  GPIO_Pin_6  /* GPIOA */
#define KEY_RIGHT GPIO_Pin_7  /* GPIOA */

static void key(void);

/* 按键引脚初始化 */
void key_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    AFIO->MAPR = 0X02000000; // 关闭 JTAG，仅保留 SWD
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_4 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU; // 输入上拉
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_11;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

/* 遥控数据初始化：读取 EEPROM 微调值 + 按键引脚
 * EEPROM 自检失败时微调清零（避免读回随机值污染中位），本次开机微调不可用 */
void RC_INIT(void)
{
    if (g_boot_fault & FAULT_EEPROM)
    {
        offset.roll  = 0;
        offset.pitch = 0;
    }
    else
    {
        offset.roll  = read_AT24C02(ROLL_ADDR_OFFSET);  // 读取 Roll 微调
        offset.pitch = read_AT24C02(PITCH_ADDR_OFFSET); // 读取 Pitch 微调
    }
    key_init();
}

/* 发送帧：帧头 0xAA 0xAF + 8 通道大端 + 末字节校验和（前 31 字节求和） */
static uint8_t tx_data[32] = {0xAA, 0xAF, 0x03, 32 - 5};
static void NRF_SEND(void)
{
    uint8_t i;

    if (g_boot_fault & FAULT_NRF)
        return; /* NRF 不在位：跳过发送，否则 TxPacket 死等 IRQ 只能靠 IWDG 复位兜底 */

    for (i = 4; i < 26; i += 2)
    {
        tx_data[i]     = *((uint8_t *)&Remote + i - 3); // 高位在前
        tx_data[i + 1] = *((uint8_t *)&Remote + i - 4); // 低位在后
    }
    tx_data[31] = 0;
    for (i = 0; i < 31; i++)
        tx_data[31] += tx_data[i]; // 校验和
    NRF24L01_TxPacket(tx_data);    // 发送到飞控端
}

/* 通道值钳位到 [1000, 2000] */
static uint16_t rc_clamp(uint16_t v)
{
    if (v > 2000)
        return 2000;
    if (v < 1000)
        return 1000;
    return v;
}

/* 遥控数据解析：10ms 调用一次 */
void RC_Analy(void)
{
    static uint16_t last_thr, last_roll, last_pitch, last_yaw;

    /* ADC_ConvertedValue 由 DMA 循环刷新，直接读取 */
    Remote.thr = (uint16_t)(0.20f * ADC_ConvertedValue[1]) + 1000 + offset.thr; // 油门
    Remote.thr = (uint16_t)(Remote.thr * 0.25f + 0.75f * last_thr);             // 一阶低通
    last_thr   = Remote.thr;

    Remote.pitch = 1000 + (uint16_t)(0.25f * ADC_ConvertedValue[2]) + offset.pitch; // 俯仰
    Remote.pitch = (uint16_t)(Remote.pitch * 0.25f + 0.75f * last_pitch);
    last_pitch   = Remote.pitch;

    Remote.yaw = 1000 + (uint16_t)(0.25f * ADC_ConvertedValue[0]) + offset.yaw; // 偏航
    Remote.yaw = (uint16_t)(Remote.yaw * 0.25f + 0.75f * last_yaw);
    last_yaw   = Remote.yaw;

    Remote.roll = 1000 + (uint16_t)(0.25f * ADC_ConvertedValue[3]) + offset.roll; // 横滚
    Remote.roll = (uint16_t)(Remote.roll * 0.25f + 0.75f * last_roll);
    last_roll   = Remote.roll;

    Remote.thr   = rc_clamp(Remote.thr);
    Remote.pitch = rc_clamp(Remote.pitch);
    Remote.roll  = rc_clamp(Remote.roll);
    Remote.yaw   = rc_clamp(Remote.yaw);

    key();      // 按键扫描
    NRF_SEND(); // 发送到飞控端
}

/* 全部按键（4 AUX + 4 微调）同时按下的检测 */
static uint8_t keys_all_pressed(void)
{
    return ((GPIOA->IDR & (GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_15)) == (GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_15)) && ((GPIOB->IDR & (GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_11)) == (GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_11)) && ((GPIOC->IDR & (GPIO_Pin_14 | GPIO_Pin_15)) == (GPIO_Pin_14 | GPIO_Pin_15));
}

/* 按键扫描（状态机） */
static void key(void)
{
    static uint8_t status = 0;
    static uint32_t temp;

    switch (status)
    {
        case 0: // 等待 300ms 全键按下，进入按键设置状态
            if (SysTick_count - temp > 30)
            {
                if (keys_all_pressed())
                    status = 1;
                BEEP_L;
            }
            break;
        case 1: // 等待松开任一按键
            if (!keys_all_pressed())
                status = 2;
            break;
        case 2: // 识别单个按键并执行
            if (!(GPIOA->IDR & KEY_AUX1))
                Remote.AUX1 = (Remote.AUX1 == 1000) ? 2000 : 1000; // 定高开关
            else if (!(GPIOB->IDR & KEY_AUX2))
                Remote.AUX2 = (Remote.AUX2 == 1000) ? 2000 : 1000; // 定点开关
            else if (!(GPIOC->IDR & KEY_AUX3))
                Remote.AUX3 = (Remote.AUX3 == 1000) ? 2000 : 1000; // 预留
            else if (!(GPIOC->IDR & KEY_AUX4))
                Remote.AUX4 = (Remote.AUX4 == 1000) ? 2000 : 1000; // 预留
            else if (!(GPIOA->IDR & KEY_LEFT))
            {
                if (offset.roll > -250)
                {
                    offset.roll -= 5;
                    write_AT24C02(ROLL_ADDR_OFFSET, offset.roll); // 记录 Roll 微调
                }
            }
            else if (!(GPIOA->IDR & KEY_RIGHT))
            {
                if (offset.roll < 250)
                {
                    offset.roll += 5;
                    write_AT24C02(ROLL_ADDR_OFFSET, offset.roll);
                }
            }
            else if (!(GPIOB->IDR & KEY_FRONT))
            {
                if (offset.pitch > -250)
                {
                    offset.pitch -= 5;
                    write_AT24C02(PITCH_ADDR_OFFSET, offset.pitch); // 记录 Pitch 微调
                }
            }
            else if (!(GPIOB->IDR & KEY_BACK))
            {
                if (offset.pitch < 250)
                {
                    offset.pitch += 5;
                    write_AT24C02(PITCH_ADDR_OFFSET, offset.pitch);
                }
            }
            status = 0;
            BEEP_H; // 按键反馈
            temp = SysTick_count;
            break;
    }
}

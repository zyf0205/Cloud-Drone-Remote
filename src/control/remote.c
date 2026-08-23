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

/* ---------------- 通道量程与摇杆映射 ---------------- */
#define RC_CH_MIN    1000u  /* 通道量程下限 */
#define RC_CH_MAX    2000u  /* 通道量程上限 */
#define STICK_SCALE 0.25f  /* ADC(0~4095)→1000~2000 满量程映射系数 */
#define THR_SCALE   0.20f  /* 油门缩放,满杆约 1819。
                              ⚠ 勿改 0.25:定高死区上限对应通道值 1750,
                              0.20 时需 92% 行程才触发上升指令,0.25 时 73% 即触发——
                              常规悬停杆位会被定高模式解释为持续上升,
                              实测"回中定不住、持续上飞"(已回退验证注记) */
#define RC_LPF_ALPHA 0.25f  /* 通道一阶低通系数(0.25 新值 + 0.75 旧值) */

/* ADC DMA 缓冲通道映射(见 adc.c) */
#define ADC_CH_YAW   0
#define ADC_CH_THR   1
#define ADC_CH_PITCH 2
#define ADC_CH_ROLL  3

/* ---------------- 发送帧格式(与飞控端 RC_Analy 严格一致) ---------------- */
#define RC_FRAME_HEAD0     0xAA /* 帧头字节0 */
#define RC_FRAME_HEAD1     0xAF /* 帧头字节1 */
#define RC_FRAME_LEN       32   /* 帧长(含校验和) */
#define RC_CHECKSUM_OFFSET 31   /* 校验和字节位置 */

/* ---------------- 按键/微调参数 ---------------- */
#define KEY_DEBOUNCE_MS 30  /* 组合键检测节拍(ms) */
#define TRIM_STEP       5   /* 微调单次步进 */
#define TRIM_LIMIT      250 /* 微调范围 ±250 */

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

static uint8_t tx_data[RC_FRAME_LEN];

/* 大端写入一个通道值 */
static void frame_put_u16(uint8_t pos, uint16_t v)
{
    tx_data[pos]     = (uint8_t)(v >> 8);
    tx_data[pos + 1] = (uint8_t)v;
}

/* 组帧发送:帧头 + 8 通道大端(字节 4~19) + 保留区填 0 + 末字节校验和(前 31 字节求和) */
static void NRF_SEND(void)
{
    uint8_t i;
    uint16_t sum = 0;

    if (g_boot_fault & FAULT_NRF)
        return; /* NRF 不在位：跳过发送，否则 TxPacket 死等 IRQ 只能靠 IWDG 复位兜底 */

    tx_data[0] = RC_FRAME_HEAD0;
    tx_data[1] = RC_FRAME_HEAD1;
    tx_data[2] = 0x03; /* 保留字节,与接收端约定 */
    tx_data[3] = 0x1B;
    frame_put_u16(4, Remote.roll);
    frame_put_u16(6, Remote.pitch);
    frame_put_u16(8, Remote.thr);
    frame_put_u16(10, Remote.yaw);
    frame_put_u16(12, Remote.AUX1);
    frame_put_u16(14, Remote.AUX2);
    frame_put_u16(16, Remote.AUX3);
    frame_put_u16(18, Remote.AUX4);
    for (i = 20; i < RC_CHECKSUM_OFFSET; i++)
        tx_data[i] = 0; /* 保留区固定 0(旧实现误读结构体外内存填充,已修复) */

    for (i = 0; i < RC_CHECKSUM_OFFSET; i++)
        sum += tx_data[i];
    tx_data[RC_CHECKSUM_OFFSET] = (uint8_t)sum;

    NRF24L01_TxPacket(tx_data); // 发送到飞控端
}

/* 通道值钳位到 [RC_CH_MIN, RC_CH_MAX] */
static uint16_t rc_clamp(uint16_t v)
{
    if (v > RC_CH_MAX)
        return RC_CH_MAX;
    if (v < RC_CH_MIN)
        return RC_CH_MIN;
    return v;
}

/* 摇杆采集:ADC 映射 + 微调偏移 + 一阶低通 + 钳位 */
static void stick_update(volatile uint16_t *ch, uint16_t adc, float scale, int16_t trim, volatile uint16_t *last)
{
    *ch   = RC_CH_MIN + (uint16_t)(scale * adc) + trim;
    *ch   = (uint16_t)(*ch * RC_LPF_ALPHA + (1.0f - RC_LPF_ALPHA) * *last);
    *last = *ch;
    *ch   = rc_clamp(*ch);
}

/* 遥控数据解析：10ms 调用一次 */
void RC_Analy(void)
{
    static uint16_t last_thr, last_roll, last_pitch, last_yaw;

    /* ADC_ConvertedValue 由 DMA 循环刷新，直接读取 */
    stick_update(&Remote.thr, ADC_ConvertedValue[ADC_CH_THR], THR_SCALE, offset.thr, &last_thr);
    stick_update(&Remote.pitch, ADC_ConvertedValue[ADC_CH_PITCH], STICK_SCALE, offset.pitch, &last_pitch);
    stick_update(&Remote.yaw, ADC_ConvertedValue[ADC_CH_YAW], STICK_SCALE, offset.yaw, &last_yaw);
    stick_update(&Remote.roll, ADC_ConvertedValue[ADC_CH_ROLL], STICK_SCALE, offset.roll, &last_roll);

    key();      // 按键扫描
    NRF_SEND(); // 发送到飞控端
}

/* 全部按键（4 AUX + 4 微调）同时按下的检测 */
static uint8_t keys_all_pressed(void)
{
    return ((GPIOA->IDR & (GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_15)) == (GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_15)) && ((GPIOB->IDR & (GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_11)) == (GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_11)) && ((GPIOC->IDR & (GPIO_Pin_14 | GPIO_Pin_15)) == (GPIO_Pin_14 | GPIO_Pin_15));
}

/* ---------------- 按键状态机 ----------------
 * 组合进入:8 键全部按住 → 松开其余保留目标键 → 执行对应功能
 * IDLE(30ms 节拍检测全键按下) → WAIT_RELEASE → EXECUTE(单次) → IDLE */
typedef enum
{
    KEY_IDLE,         /* 空闲:检测 8 键全按的组合手势 */
    KEY_WAIT_RELEASE, /* 组合已确认:等用户松开其余按键 */
    KEY_EXECUTE       /* 识别仍按住的目标键并执行一次 */
} KeyState;

/* 按键扫描(10ms 调用) */
static void key(void)
{
    static KeyState status = KEY_IDLE;
    static uint32_t temp;

    switch (status)
    {
        case KEY_IDLE:
            if (SysTick_count - temp > KEY_DEBOUNCE_MS)
            {
                if (keys_all_pressed())
                    status = KEY_WAIT_RELEASE;
                BEEP_L;
            }
            break;
        case KEY_WAIT_RELEASE:
            if (!keys_all_pressed())
                status = KEY_EXECUTE;
            break;
        case KEY_EXECUTE:
            /* AUX 开关:每执行一次在 开(2000)/关(1000) 间翻转 */
            if (!(GPIOA->IDR & KEY_AUX1))
                Remote.AUX1 = (Remote.AUX1 == RC_CH_MIN) ? RC_CH_MAX : RC_CH_MIN; // 定高开关
            else if (!(GPIOB->IDR & KEY_AUX2))
                Remote.AUX2 = (Remote.AUX2 == RC_CH_MIN) ? RC_CH_MAX : RC_CH_MIN; // 定点开关
            else if (!(GPIOC->IDR & KEY_AUX3))
                Remote.AUX3 = (Remote.AUX3 == RC_CH_MIN) ? RC_CH_MAX : RC_CH_MIN; // 陀螺校准触发(飞控端)
            else if (!(GPIOC->IDR & KEY_AUX4))
                Remote.AUX4 = (Remote.AUX4 == RC_CH_MIN) ? RC_CH_MAX : RC_CH_MIN; // 预留
            else if (!(GPIOA->IDR & KEY_LEFT))
            {
                if (offset.roll > -TRIM_LIMIT)
                {
                    offset.roll -= TRIM_STEP;
                    write_AT24C02(ROLL_ADDR_OFFSET, offset.roll); // 微调即时写 EEPROM
                }
            }
            else if (!(GPIOA->IDR & KEY_RIGHT))
            {
                if (offset.roll < TRIM_LIMIT)
                {
                    offset.roll += TRIM_STEP;
                    write_AT24C02(ROLL_ADDR_OFFSET, offset.roll);
                }
            }
            else if (!(GPIOB->IDR & KEY_FRONT))
            {
                if (offset.pitch > -TRIM_LIMIT)
                {
                    offset.pitch -= TRIM_STEP;
                    write_AT24C02(PITCH_ADDR_OFFSET, offset.pitch);
                }
            }
            else if (!(GPIOB->IDR & KEY_BACK))
            {
                if (offset.pitch < TRIM_LIMIT)
                {
                    offset.pitch += TRIM_STEP;
                    write_AT24C02(PITCH_ADDR_OFFSET, offset.pitch);
                }
            }
            status = KEY_IDLE;
            BEEP_H; // 按键反馈
            temp = SysTick_count;
            break;
    }
}

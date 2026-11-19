/*******************************************************************
 * 遥控器 8 路按键板级驱动与去抖状态机实现
 *
 * 交互行为说明（与硬件真实手感一致）：
 *   每个按键均为独立输入上拉（按下为低电平 0，松开为高电平 1）。
 *   由 Task_Input 每 20ms 调用一次 Key_Scan()：
 *     1. IDLE         : 检测到有键按下 → 记录候选键并进入 DEBOUNCE；
 *     2. DEBOUNCE     : 20ms 后该键仍保持按下 → 确认有效，输出单次事件，
 *                       进入 WAIT_RELEASE（若中途抖动弹起则退回 IDLE）；
 *     3. WAIT_RELEASE : 等待全部 8 键完全松开（pressed_mask == 0）再回 IDLE，
 *                       确保单次按压只触发一次动作，不会误连发。
 ******************************************************************/
#include "stm32f10x.h"
#include "key.h"

/* 硬件按键引脚映射 */
#define PIN_KEY_AUX1  GPIO_Pin_15 /* PA15 */
#define PIN_KEY_AUX2  GPIO_Pin_11 /* PB11 */
#define PIN_KEY_AUX3  GPIO_Pin_14 /* PC14 */
#define PIN_KEY_AUX4  GPIO_Pin_15 /* PC15 */
#define PIN_KEY_LEFT  GPIO_Pin_6  /* PA6 */
#define PIN_KEY_RIGHT GPIO_Pin_7  /* PA7 */
#define PIN_KEY_FRONT GPIO_Pin_0  /* PB0 */
#define PIN_KEY_BACK  GPIO_Pin_1  /* PB1 */

static key_fsm_t s_key_fsm;

/* 将按键掩码按固定优先级翻译为离散事件 */
static key_event_t key_mask_to_event(uint8_t mask)
{
    if (mask & KEY_MASK_AUX1)
        return KEY_EVT_AUX1;
    if (mask & KEY_MASK_AUX2)
        return KEY_EVT_AUX2;
    if (mask & KEY_MASK_AUX3)
        return KEY_EVT_AUX3;
    if (mask & KEY_MASK_AUX4)
        return KEY_EVT_AUX4;
    if (mask & KEY_MASK_TRIM_LEFT)
        return KEY_EVT_TRIM_LEFT;
    if (mask & KEY_MASK_TRIM_RIGHT)
        return KEY_EVT_TRIM_RIGHT;
    if (mask & KEY_MASK_TRIM_FRONT)
        return KEY_EVT_TRIM_FRONT;
    if (mask & KEY_MASK_TRIM_BACK)
        return KEY_EVT_TRIM_BACK;
    return KEY_EVT_NONE;
}

/* 初始化 8 路按键 GPIO（释放 JTAG 保留 SWD 以启用 PA15） */
void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO,
                           ENABLE);

    /* 关闭 JTAG-DP，仅保留 SW-DP，释放 PA15 作为 AUX1 普通输入脚 */
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU; /* 内部上拉，按下接地为低 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_InitStructure.GPIO_Pin = PIN_KEY_LEFT | PIN_KEY_RIGHT | PIN_KEY_AUX1;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = PIN_KEY_FRONT | PIN_KEY_BACK | PIN_KEY_AUX2;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = PIN_KEY_AUX3 | PIN_KEY_AUX4;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    Key_FsmInit(&s_key_fsm);
}

/* 读取 8 路按键实时按下状态（返回位掩码，bit=1 表示对应按键处于低电平按下态） */
uint8_t Key_ReadPressedMask(void)
{
    uint8_t mask  = 0u;
    uint16_t idra = GPIOA->IDR;
    uint16_t idrb = GPIOB->IDR;
    uint16_t idrc = GPIOC->IDR;

    if ((idra & PIN_KEY_AUX1) == 0u)
        mask |= KEY_MASK_AUX1;
    if ((idrb & PIN_KEY_AUX2) == 0u)
        mask |= KEY_MASK_AUX2;
    if ((idrc & PIN_KEY_AUX3) == 0u)
        mask |= KEY_MASK_AUX3;
    if ((idrc & PIN_KEY_AUX4) == 0u)
        mask |= KEY_MASK_AUX4;
    if ((idra & PIN_KEY_LEFT) == 0u)
        mask |= KEY_MASK_TRIM_LEFT;
    if ((idra & PIN_KEY_RIGHT) == 0u)
        mask |= KEY_MASK_TRIM_RIGHT;
    if ((idrb & PIN_KEY_FRONT) == 0u)
        mask |= KEY_MASK_TRIM_FRONT;
    if ((idrb & PIN_KEY_BACK) == 0u)
        mask |= KEY_MASK_TRIM_BACK;

    return mask;
}

/* 复位按键状态机 */
void Key_FsmInit(key_fsm_t *fsm)
{
    fsm->state          = KEY_FSM_IDLE;
    fsm->candidate_mask = 0u;
}

/*
 * 纯逻辑状态机步进（每 20ms 调用一次，无硬件依赖，便于离线单元测试）：
 *   pressed_mask：当前时刻 8 路按键按下掩码（1=按下，0=松开）
 */
key_event_t Key_FsmStep(key_fsm_t *fsm, uint8_t pressed_mask)
{
    switch (fsm->state)
    {
        case KEY_FSM_IDLE:
            if (pressed_mask != 0u)
            {
                fsm->candidate_mask = pressed_mask;
                fsm->state          = KEY_FSM_DEBOUNCE;
            }
            break;

        case KEY_FSM_DEBOUNCE:
            /* 经过一个 20ms 扫描周期后，候选按键依然稳定按下才判定为有效事件 */
            if ((pressed_mask & fsm->candidate_mask) != 0u)
            {
                key_event_t evt = key_mask_to_event((uint8_t)(pressed_mask & fsm->candidate_mask));
                fsm->state      = KEY_FSM_WAIT_RELEASE;
                return evt;
            }
            /* 抖动毛刺，退回空闲态 */
            fsm->candidate_mask = 0u;
            fsm->state          = KEY_FSM_IDLE;
            break;

        case KEY_FSM_WAIT_RELEASE:
            if (pressed_mask == 0u)
            {
                fsm->candidate_mask = 0u;
                fsm->state          = KEY_FSM_IDLE;
            }
            break;

        default:
            Key_FsmInit(fsm);
            break;
    }

    return KEY_EVT_NONE;
}

/* 硬件扫描入口（由 Task_Input 每 20ms 周期调用） */
key_event_t Key_Scan(void)
{
    return Key_FsmStep(&s_key_fsm, Key_ReadPressedMask());
}

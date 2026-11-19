/*******************************************************************
 * NRF24L01+ 无线模块板级驱动（遥控器发送端，单向 TX 模式）
 *
 * 引脚分配：
 *   控制脚：CE = PA11，CSN = PB12，IRQ = PA8（低电平有效）
 *   软件SPI：PB13 = SCK，PB14 = MISO，PB15 = MOSI
 *
 * 与飞控端对接参数（必须两端严格一致，详见 docs/02_遥控协议说明.md）：
 *   RF_CH = 1、地址 {0xE1,0xE2,0xE3,0xE4,0xE5}、32B 载荷、无 ACK、1Mbps / 0dBm
 *
 * 所有权说明：
 *   调度器启动后仅由 Task_RC_Tx 独占访问，无需额外总线锁。
 ******************************************************************/
#include "nrf24l01.h"
#include "spi.h"
#include "delay.h"

/* 等待单帧发送完成（IRQ 拉低）的软件超时上限（1Mbps 发 32B 实测 < 0.6ms） */
#define NRF_TX_TIMEOUT_MS 5u
/* 上电在位检测重试次数与间隔（20 × 50ms = 1s 窗口，兼容电源爬坡较慢的模块） */
#define NRF_INIT_RETRY_MAX   20u
#define NRF_INIT_RETRY_MS    50u

/* 射频地址（与飞控端 RX_ADDR_P0 严格一致） */
static const uint8_t TX_ADDRESS[TX_ADR_WIDTH] = {0xE1, 0xE2, 0xE3, 0xE4, 0xE5};
static const uint8_t RX_ADDRESS[RX_ADR_WIDTH] = {0xE1, 0xE2, 0xE3, 0xE4, 0xE5};

#define Set_NRF24L01_CSN  (GPIOB->BSRR = GPIO_Pin_12)
#define Clr_NRF24L01_CSN  (GPIOB->BRR = GPIO_Pin_12)
#define Set_NRF24L01_CE   (GPIOA->BSRR = GPIO_Pin_11)
#define Clr_NRF24L01_CE   (GPIOA->BRR = GPIO_Pin_11)
#define READ_NRF24L01_IRQ (GPIOA->IDR & GPIO_Pin_8)

/* 初始化 NRF 控制引脚与底层软件 SPI */
void NRF24L01_Configuration(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    /* CE (PA11) 推挽输出 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* CSN (PB12) 推挽输出 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* IRQ (PA8) 上拉输入（发送完成时模块将其拉低） */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_SetBits(GPIOA, GPIO_Pin_8);

    SPI_Config();
    Clr_NRF24L01_CE;
    Set_NRF24L01_CSN;
}

/* 写单个寄存器，返回当前 STATUS */
u8 NRF24L01_Write_Reg(u8 regaddr, u8 data)
{
    u8 status;

    Clr_NRF24L01_CSN;
    status = SPI_RW(regaddr);
    SPI_RW(data);
    Set_NRF24L01_CSN;
    return status;
}

/* 读单个寄存器 */
u8 NRF24L01_Read_Reg(u8 regaddr)
{
    u8 reg_val;

    Clr_NRF24L01_CSN;
    SPI_RW(regaddr);
    reg_val = SPI_RW(0xFFu);
    Set_NRF24L01_CSN;
    return reg_val;
}

/* 连续读取寄存器/FIFO 定长数据 */
u8 NRF24L01_Read_Buf(u8 regaddr, u8 *pBuf, u8 datalen)
{
    u8 status;
    u8 i;

    Clr_NRF24L01_CSN;
    status = SPI_RW(regaddr);
    for (i = 0; i < datalen; i++)
    {
        pBuf[i] = SPI_RW(0xFFu);
    }
    Set_NRF24L01_CSN;
    return status;
}

/* 连续写入寄存器/FIFO 定长数据 */
u8 NRF24L01_Write_Buf(u8 regaddr, const u8 *pBuf, u8 datalen)
{
    u8 status;
    u8 i;

    Clr_NRF24L01_CSN;
    status = SPI_RW(regaddr);
    for (i = 0; i < datalen; i++)
    {
        SPI_RW(pBuf[i]);
    }
    Set_NRF24L01_CSN;
    return status;
}

/* 检测 NRF24L01+ 是否在位：向 TX_ADDR 写 5 字节 0xA5 并读回比对 */
u8 NRF24L01_Check(void)
{
    static const u8 pattern[TX_ADR_WIDTH] = {0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u};
    u8 readback[TX_ADR_WIDTH];
    u8 i;

    NRF24L01_Write_Buf(SPI_WRITE_REG + TX_ADDR, pattern, TX_ADR_WIDTH);
    NRF24L01_Read_Buf(TX_ADDR, readback, TX_ADR_WIDTH);
    for (i = 0; i < TX_ADR_WIDTH; i++)
    {
        if (readback[i] != 0xA5u)
            return 1u; /* 不在位或 SPI 异常 */
    }
    return 0u; /* 在位正常 */
}

/* 配置为单向 TX 模式（1Mbps / 0dBm / RF_CH=1 / 无自动应答 / CRC16） */
void TX_Mode(void)
{
    Clr_NRF24L01_CE;
    NRF24L01_Write_Buf(SPI_WRITE_REG + TX_ADDR, TX_ADDRESS, TX_ADR_WIDTH);
    NRF24L01_Write_Buf(SPI_WRITE_REG + RX_ADDR_P0, RX_ADDRESS, RX_ADR_WIDTH);

    NRF24L01_Write_Reg(SPI_WRITE_REG + EN_AA, 0x00u);      /* 禁用自动应答（单向链路） */
    NRF24L01_Write_Reg(SPI_WRITE_REG + EN_RXADDR, 0x01u);  /* 使能通道 0 地址 */
    NRF24L01_Write_Reg(SPI_WRITE_REG + SETUP_RETR, 0x1Au); /* 保留与原版一致配置 */
    NRF24L01_Write_Reg(SPI_WRITE_REG + RF_CH, 1u);         /* RF 频道 1 */
    NRF24L01_Write_Reg(SPI_WRITE_REG + RF_SETUP, 0x07u);   /* 1Mbps, 0dBm */
    NRF24L01_Write_Reg(SPI_WRITE_REG + NCONFIG, 0x0Eu);    /* PWR_UP, CRC16, PRIM_TX */
    NRF24L01_Write_Reg(FLUSH_TX, 0xFFu);                   /* 清空残留 TX FIFO */
    NRF24L01_Write_Reg(SPI_WRITE_REG + STATUS, 0x70u);     /* 清除挂起中断位 */
    Set_NRF24L01_CE;
}

/*
 * 发送一帧 32 字节数据（带软件超时保护）：
 *   写 TX FIFO → 拉高 CE 触发发射 → 等待 IRQ 拉低（最多 NRF_TX_TIMEOUT_MS）
 *   超时时自动拉低 CE、清空 TX FIFO 并清除状态位，防止任务死等卡死。
 */
u8 NRF24L01_TxPacket(const u8 *txbuf)
{
    u8 state;
    uint32_t start_ms;

    Clr_NRF24L01_CE;
    NRF24L01_Write_Buf(WR_TX_PLOAD, txbuf, TX_PLOAD_WIDTH);
    Set_NRF24L01_CE;

    start_ms = get_ms_tick();
    while (READ_NRF24L01_IRQ != 0u)
    {
        if ((uint32_t)(get_ms_tick() - start_ms) >= NRF_TX_TIMEOUT_MS)
        {
            Clr_NRF24L01_CE;
            NRF24L01_Write_Reg(FLUSH_TX, 0xFFu);
            NRF24L01_Write_Reg(SPI_WRITE_REG + STATUS, 0x70u);
            return NRF_TX_TIMEOUT;
        }
    }

    state = NRF24L01_Read_Reg(STATUS);
    NRF24L01_Write_Reg(SPI_WRITE_REG + STATUS, state); /* 清除 TX_DS / MAX_RT */

    if (state & MAX_TX)
    {
        NRF24L01_Write_Reg(FLUSH_TX, 0xFFu);
        return MAX_TX;
    }
    if (state & TX_OK)
    {
        return TX_OK;
    }
    return NRF_TX_ERR;
}

/* 上电初始化：IO → 带超时重试的在位检测 → 配置 TX 模式（0=正常，1=失败） */
u8 NRF24L01_init(void)
{
    u8 retry = NRF_INIT_RETRY_MAX;

    NRF24L01_Configuration();
    while (retry > 0u)
    {
        if (NRF24L01_Check() == 0u)
            break;
        retry--;
        delay_ms(NRF_INIT_RETRY_MS);
    }

    if (retry == 0u)
        return 1u;

    TX_Mode();
    return 0u;
}

/*******************************************************************
 * NRF24L01 无线模块驱动（遥控器端 = 发送端，TX 模式）
 * 控制脚：CE = PA11, CSN = PB12, IRQ = PA8
 * SPI（软件模拟）：PB13 = SCK, PB14 = MISO, PB15 = MOSI
 *
 * RF 参数：RF_CH=1、地址 {E1,E2,E3,E4,E5}、32B 载荷、无 ACK、1Mbps
 * 必须与飞控端接收配置一致，详见 docs/02_遥控协议说明.md
 ******************************************************************/
#include "nrf24l01.h"
#include "spi.h"
#include "delay.h"
#include <string.h>

/* 发送地址（与飞控端 RX_ADDR_P0 相同） */
const uint8_t TX_ADDRESS[] = {0xE1, 0xE2, 0xE3, 0xE4, 0xE5};
const uint8_t RX_ADDRESS[] = {0xE1, 0xE2, 0xE3, 0xE4, 0xE5};

#define Set_NRF24L01_CSN  (GPIOB->BSRR = GPIO_Pin_12) // PB12
#define Clr_NRF24L01_CSN  (GPIOB->BRR = GPIO_Pin_12)
#define Set_NRF24L01_CE   (GPIOA->BSRR = GPIO_Pin_11) // PA11
#define Clr_NRF24L01_CE   (GPIOA->BRR = GPIO_Pin_11)
#define READ_NRF24L01_IRQ (GPIOA->IDR & GPIO_Pin_8) // PA8，低电平有效

/* NRF24L01 IO 初始化 */
void NRF24L01_Configuration(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_11; // CE
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_12; // CSN
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    Set_NRF24L01_CE;
    Set_NRF24L01_CSN;

    /* IRQ 输入上拉 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_SetBits(GPIOA, GPIO_Pin_8);

    SPI_Config(); // 初始化软件 SPI
    Clr_NRF24L01_CE;
    Set_NRF24L01_CSN;
}

/* 检测 NRF24L01 是否在位：写读 5 字节比对 */
u8 NRF24L01_Check(void)
{
    u8 buf[5] = {0XA5, 0XA5, 0XA5, 0XA5, 0XA5};
    u8 buf1[5];
    u8 i;

    NRF24L01_Write_Buf(SPI_WRITE_REG + TX_ADDR, buf, 5);
    NRF24L01_Read_Buf(TX_ADDR, buf1, 5);
    for (i = 0; i < 5; i++)
        if (buf1[i] != 0XA5)
            break;
    if (i != 5)
        return 1; // 不在位
    return 0;     // 在位
}

/* 写寄存器 */
u8 NRF24L01_Write_Reg(u8 regaddr, u8 data)
{
    u8 status;
    Clr_NRF24L01_CSN;
    status = SPI_RW(regaddr);
    SPI_RW(data);
    Set_NRF24L01_CSN;
    return status;
}

/* 读寄存器 */
u8 NRF24L01_Read_Reg(u8 regaddr)
{
    u8 reg_val;
    Clr_NRF24L01_CSN;
    SPI_RW(regaddr);
    reg_val = SPI_RW(0XFF);
    Set_NRF24L01_CSN;
    return reg_val;
}

/* 读定长数据 */
u8 NRF24L01_Read_Buf(u8 regaddr, u8 *pBuf, u8 datalen)
{
    u8 status, u8_ctr;
    Clr_NRF24L01_CSN;
    status = SPI_RW(regaddr);
    for (u8_ctr = 0; u8_ctr < datalen; u8_ctr++)
        pBuf[u8_ctr] = SPI_RW(0XFF);
    Set_NRF24L01_CSN;
    return status;
}

/* 写定长数据 */
u8 NRF24L01_Write_Buf(u8 regaddr, u8 *pBuf, u8 datalen)
{
    u8 status, u8_ctr;
    Clr_NRF24L01_CSN;
    status = SPI_RW(regaddr);
    for (u8_ctr = 0; u8_ctr < datalen; u8_ctr++)
        SPI_RW(*pBuf++);
    Set_NRF24L01_CSN;
    return status;
}

/* 发送一帧数据：写 TX FIFO → CE 拉高 → 等待 IRQ 拉低（发送完成） */
u8 NRF24L01_TxPacket(u8 *txbuf)
{
    u8 state;
    Clr_NRF24L01_CE;
    NRF24L01_Write_Buf(WR_TX_PLOAD, txbuf, TX_PLOAD_WIDTH);
    Set_NRF24L01_CE;
    while (READ_NRF24L01_IRQ != 0); // 等待发送完成
    state = NRF24L01_Read_Reg(STATUS);
    NRF24L01_Write_Reg(SPI_WRITE_REG + STATUS, state); // 清 TX_DS / MAX_RT 标志
    if (state & MAX_TX)                                // 达到最大重发次数
    {
        NRF24L01_Write_Reg(FLUSH_TX, 0xff); // 清 TX FIFO
        return MAX_TX;
    }
    if (state & TX_OK) // 发送成功
    {
        return TX_OK;
    }
    return 0xff; // 未知原因失败
}

/* 配置为 TX 模式（无自动应答，1Mbps，0dBm） */
void TX_Mode(void)
{
    Clr_NRF24L01_CE;
    NRF24L01_Write_Buf(SPI_WRITE_REG + TX_ADDR, (u8 *)TX_ADDRESS, TX_ADR_WIDTH);
    NRF24L01_Write_Buf(SPI_WRITE_REG + RX_ADDR_P0, (u8 *)RX_ADDRESS, RX_ADR_WIDTH);

    NRF24L01_Write_Reg(SPI_WRITE_REG + EN_AA, 0x00);     // 无自动应答
    NRF24L01_Write_Reg(SPI_WRITE_REG + EN_RXADDR, 0x01); // 使能通道 0 接收地址
    NRF24L01_Write_Reg(SPI_WRITE_REG + SETUP_RETR, 0x1a);
    NRF24L01_Write_Reg(SPI_WRITE_REG + RF_CH, 1); // RF 通道 1
    NRF24L01_Write_Reg(SPI_WRITE_REG + RF_SETUP, 0x07);
    NRF24L01_Write_Reg(SPI_WRITE_REG + NCONFIG, 0x0e); // PWR_UP, CRC16, PRIM_TX
    Set_NRF24L01_CE;                                   // CE 拉高，等待发送
}

/* 初始化：IO → 在位检测（带重试上界）→ TX 模式
 * 返回 0=正常，1=自检失败（不再死等，由调用方置故障标志报警） */
u8 NRF24L01_init(void)
{
    u8 retry = 20; /* 20 次 × 50ms = 1s 等待窗口，覆盖模块上电较慢的情况 */

    NRF24L01_Configuration();
    while (retry > 0)
    {
        if (NRF24L01_Check() == 0)
            break;
        retry--;
        delay_ms(50);
    }
    TX_Mode();
    return (retry == 0) ? 1 : 0;
}

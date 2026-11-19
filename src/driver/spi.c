/*******************************************************************
 * 软件模拟 SPI 总线驱动（NRF24L01+ 专用）
 *
 * 引脚：PB13 = SCK，PB14 = MISO，PB15 = MOSI
 * 时序：MSB 先行，SPI Mode 0（空闲时钟低电平，上升沿采样）
 * 注意：调度器启动后仅由 Task_RC_Tx 通过 NRF24L01 驱动调用，无并发冲突。
 ******************************************************************/
#include "spi.h"

#define SCK_H  (GPIOB->BSRR = GPIO_Pin_13)
#define SCK_L  (GPIOB->BRR = GPIO_Pin_13)
#define MOSI_H (GPIOB->BSRR = GPIO_Pin_15)
#define MOSI_L (GPIOB->BRR = GPIO_Pin_15)
#define MISO   ((GPIOB->IDR & GPIO_Pin_14) ? 1u : 0u)

/* 初始化软件 SPI 引脚（片选 CSN PB12 由 nrf24l01.c 管理） */
void SPI_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_SetBits(GPIOB, GPIO_Pin_12); /* 预置 CSN 为高，避免总线误选中 */

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_13 | GPIO_Pin_15; /* SCK, MOSI */
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_14; /* MISO */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    SCK_L;
    MOSI_H;
}

/* 全双工收发 1 字节（发送 byte 的同时返回从机移回的字节） */
u8 SPI_RW(u8 byte)
{
    uint8_t i;
    u8 rx_val = 0x00u;

    for (i = 0; i < 8; i++)
    {
        SCK_L;
        if (byte & 0x80u)
            MOSI_H;
        else
            MOSI_L;
        byte <<= 1;
        rx_val <<= 1;

        SCK_H; /* 上升沿采样 MISO */
        if (MISO)
            rx_val |= 0x01u;
        SCK_L;
    }
    return rx_val;
}

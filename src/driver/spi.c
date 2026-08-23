/*******************************************************************
 * 软件 SPI 总线（NRF24L01 专用）
 * PB13 = SCK, PB14 = MISO, PB15 = MOSI，MSB first，CPOL=0/CPHA=0
 ******************************************************************/
#include "spi.h"

#define SCK_H  GPIOB->BSRR = GPIO_Pin_13
#define SCK_L  GPIOB->BRR = GPIO_Pin_13
#define MOSI_H GPIOB->BSRR = GPIO_Pin_15
#define MOSI_L GPIOB->BRR = GPIO_Pin_15
#define MISO   ((GPIOB->IDR & GPIO_Pin_14) ? 1 : 0)

/* IO 初始化（CSN 在 nrf24l01.c 中配置） */
void SPI_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_SetBits(GPIOB, GPIO_Pin_12); // NRF_CS 预置为高

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_13; // SCK
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15; // MOSI
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_14; // MISO
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    SCK_L;
    MOSI_H;
}

/* SPI 读写一个字节（同时收发） */
u8 SPI_RW(u8 byte)
{
    uint8_t i;
    u8 Temp = 0x00;

    for (i = 0; i < 8; i++)
    {
        SCK_L; // 先拉低时钟
        if (byte & 0x80)
            MOSI_H;
        else
            MOSI_L;
        byte <<= 1;
        Temp <<= 1;
        SCK_H; // 上升沿采样
        if (MISO)
            Temp++;
        SCK_L;
    }
    return Temp;
}

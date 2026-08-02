#ifndef __NRF24L01_H
#define __NRF24L01_H

#include "stm32f10x.h"

/* 地址与载荷宽度 */
#define TX_ADR_WIDTH   5  // 5 字节地址
#define RX_ADR_WIDTH   5
#define TX_PLOAD_WIDTH 32 // 32 字节用户数据

/* NRF24L01 指令 */
#define SPI_WRITE_REG 0x20 // 写配置寄存器
#define WR_TX_PLOAD   0xA0 // 写 TX 有效数据
#define FLUSH_TX      0xE1 // 清 TX FIFO

/* NRF24L01 寄存器地址 */
#define NCONFIG    0x00 // bit0: 1=PRX,0=PTX; bit1: 上电选择; bit3: CRC使能; bit4~6: 中断使能
#define EN_AA      0x01 // 自动应答使能，bit0~5 对应通道 0~5
#define EN_RXADDR  0x02 // 接收地址使能，bit0~5 对应通道 0~5
#define SETUP_RETR 0x04 // 自动重发：bit7:4 延时，bit3:0 次数
#define RF_CH      0x05 // RF 通道
#define RF_SETUP   0x06 // bit3: 速率(0=1M,1=2M); bit2: 1=最大功率
#define STATUS     0x07 // bit4: MAX_RT; bit5: TX_DS; bit6: RX_DR
#define RX_ADDR_P0 0x0A // 通道 0 接收地址
#define TX_ADDR     0x10 // 发送地址

/* 状态位 */
#define MAX_TX 0x10 // 达到最大重发次数
#define TX_OK  0x20 // 发送完成

/* 函数声明 */
void NRF24L01_Configuration(void);
void TX_Mode(void);
u8 NRF24L01_Write_Buf(u8 regaddr, u8 *pBuf, u8 datalen);
u8 NRF24L01_Read_Buf(u8 regaddr, u8 *pBuf, u8 datalen);
u8 NRF24L01_Read_Reg(u8 regaddr);
u8 NRF24L01_Write_Reg(u8 regaddr, u8 data);
u8 NRF24L01_Check(void);
u8 NRF24L01_TxPacket(u8 *txbuf);
void NRF24L01_init(void);

#endif

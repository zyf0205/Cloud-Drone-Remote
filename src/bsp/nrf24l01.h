/*******************************************************************
 * NRF24L01+ 无线发送端板级驱动头文件
 *
 * 与飞控端对接红线（严禁单方面修改）：
 *   RF_CH = 1、地址 {0xE1,0xE2,0xE3,0xE4,0xE5}、32 字节载荷、
 *   1Mbps / 0dBm、无自动应答（EN_AA = 0x00）、CRC16。
 ******************************************************************/
#ifndef __NRF24L01_H
#define __NRF24L01_H

#include "stm32f10x.h"

/* 地址与载荷宽度 */
#define TX_ADR_WIDTH   5u  /* 5 字节射频地址 */
#define RX_ADR_WIDTH   5u
#define TX_PLOAD_WIDTH 32u /* 32 字节固定载荷 */

/* NRF24L01 SPI 命令字 */
#define SPI_WRITE_REG 0x20u /* 写配置寄存器 */
#define WR_TX_PLOAD   0xA0u /* 写 TX FIFO 载荷 */
#define FLUSH_TX      0xE1u /* 清空 TX FIFO */

/* NRF24L01 寄存器地址 */
#define NCONFIG    0x00u /* bit0:0=PTX; bit1:PWR_UP; bit3:EN_CRC; bit2:CRCO(16bit) */
#define EN_AA      0x01u /* 自动应答使能 */
#define EN_RXADDR  0x02u /* 接收通道使能 */
#define SETUP_RETR 0x04u /* 自动重发配置 */
#define RF_CH      0x05u /* 射频频道 */
#define RF_SETUP   0x06u /* 速率与发射功率 */
#define STATUS     0x07u /* 中断状态寄存器 */
#define RX_ADDR_P0 0x0Au /* 通道 0 接收地址 */
#define TX_ADDR    0x10u /* 发送目标地址 */

/* 发送返回状态码 */
#define MAX_TX         0x10u /* 达到最大重发次数 */
#define TX_OK          0x20u /* 发送完成（TX_DS） */
#define NRF_TX_TIMEOUT 0xFEu /* 等待 IRQ 超时（模块异常或脱焊） */
#define NRF_TX_ERR     0xFFu /* 其他未知发送失败 */

/* 函数接口 */
extern void NRF24L01_Configuration(void);
extern void TX_Mode(void);
extern u8 NRF24L01_Write_Buf(u8 regaddr, const u8 *pBuf, u8 datalen);
extern u8 NRF24L01_Read_Buf(u8 regaddr, u8 *pBuf, u8 datalen);
extern u8 NRF24L01_Read_Reg(u8 regaddr);
extern u8 NRF24L01_Write_Reg(u8 regaddr, u8 data);
extern u8 NRF24L01_Check(void);
extern u8 NRF24L01_TxPacket(const u8 *txbuf);
extern u8 NRF24L01_init(void); /* 返回 0=正常，1=上电自检失败 */

#endif /* __NRF24L01_H */

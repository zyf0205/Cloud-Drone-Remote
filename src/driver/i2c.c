/*******************************************************************
 * 软件 I2C 总线（PB6 = SCL, PB7 = SDA）
 * 挂载 AT24C02 EEPROM，用于保存摇杆微调值
 ******************************************************************/
#include "i2c.h"

#define SUCCESS 0
#define FAILED 1

#define I2c_delay()                \
    {                              \
        volatile unsigned char i;  \
        for (i = 5; i > 0; i--)    \
            ;                      \
    }

/* I2C GPIO 初始化：开漏输出（板上有上拉电阻） */
void IIC_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStrucSUCCESS;
    GPIO_InitStrucSUCCESS.GPIO_Pin = SCL_PIN;
    GPIO_InitStrucSUCCESS.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStrucSUCCESS.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(IIC_GPIO, &GPIO_InitStrucSUCCESS);

    GPIO_InitStrucSUCCESS.GPIO_Pin = SCL_PIN | SDA_PIN;
    GPIO_InitStrucSUCCESS.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_Init(IIC_GPIO, &GPIO_InitStrucSUCCESS);
}

/* 起始信号：返回 FAILED 表示总线忙 */
static uint8_t I2c_Start(void)
{
    SDA_H;
    SCL_H;
    I2c_delay();
    if (!SDA_read)
        return FAILED;
    SDA_L;
    I2c_delay();
    if (SDA_read)
        return FAILED;
    SCL_L;
    I2c_delay();
    return SUCCESS;
}

/* 停止信号 */
static void I2c_Stop(void)
{
    SCL_L;
    I2c_delay();
    SDA_L;
    I2c_delay();
    I2c_delay();
    SCL_H;
    I2c_delay();
    SDA_H;
    I2c_delay();
}

/* 应答 */
static void I2c_Ack(void)
{
    SCL_L;
    I2c_delay();
    SDA_L;
    I2c_delay();
    SCL_H;
    I2c_delay();
    I2c_delay();
    I2c_delay();
    I2c_delay();
    SCL_L;
    I2c_delay();
}

/* 非应答 */
static void I2c_NoAck(void)
{
    SCL_L;
    I2c_delay();
    SDA_H;
    I2c_delay();
    SCL_H;
    I2c_delay();
    I2c_delay();
    I2c_delay();
    I2c_delay();
    SCL_L;
    I2c_delay();
}

/* 等待从机应答：返回 FAILED 表示无应答 */
static uint8_t I2c_WaitAck(void)
{
    SCL_L;
    I2c_delay();
    SDA_H;
    I2c_delay();
    SCL_H;
    I2c_delay();
    I2c_delay();
    I2c_delay();

    if (SDA_read)
    {
        SCL_L;
        return FAILED;
    }
    SCL_L;
    return SUCCESS;
}

/* 发送一个字节 */
static void I2c_SendByte(uint8_t byte)
{
    uint8_t i = 8;
    while (i--)
    {
        SCL_L;
        I2c_delay();
        if (byte & 0x80)
            SDA_H;
        else
            SDA_L;
        byte <<= 1;
        I2c_delay();
        SCL_H;
        I2c_delay();
        I2c_delay();
        I2c_delay();
    }
    SCL_L;
}

/* 读取一个字节 */
static uint8_t I2c_ReadByte(void)
{
    uint8_t i = 8;
    uint8_t byte = 0;

    SDA_H;
    while (i--)
    {
        byte <<= 1;
        SCL_L;
        I2c_delay();
        I2c_delay();
        SCL_H;
        I2c_delay();
        I2c_delay();
        I2c_delay();
        if (SDA_read)
            byte |= 0x01;
    }
    SCL_L;
    return byte;
}

/* 向设备寄存器写入定长数据 */
int8_t IIC_Write_Bytes(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len)
{
    int i;
    if (I2c_Start() == FAILED)
        return FAILED;
    I2c_SendByte(addr);
    if (I2c_WaitAck() == FAILED)
    {
        I2c_Stop();
        return FAILED;
    }
    I2c_SendByte(reg);
    I2c_WaitAck();
    for (i = 0; i < len; i++)
    {
        I2c_SendByte(data[i]);
        if (I2c_WaitAck() == FAILED)
        {
            I2c_Stop();
            return FAILED;
        }
    }
    I2c_Stop();
    return SUCCESS;
}

/* 读设备寄存器一个字节 */
int8_t IIC_Read_One_Byte(uint8_t addr, uint8_t reg)
{
    uint8_t recive = 0;
    if (I2c_Start() == FAILED)
        return FAILED;
    I2c_SendByte(addr);
    if (I2c_WaitAck() == FAILED)
    {
        I2c_Stop();
        return FAILED;
    }
    I2c_SendByte(reg);
    I2c_WaitAck();
    I2c_Stop();
    I2c_Start();
    I2c_SendByte(addr + 1);
    if (I2c_WaitAck() == FAILED)
    {
        I2c_Stop();
        return FAILED;
    }
    recive = I2c_ReadByte();
    I2c_NoAck();
    I2c_Stop();
    return recive;
}

/* 写设备寄存器一个字节 */
int8_t IIC_Write_One_Byte(uint8_t addr, uint8_t reg, uint8_t data)
{
    if (I2c_Start() == FAILED)
        return FAILED;
    I2c_SendByte(addr);
    if (I2c_WaitAck() == FAILED)
    {
        I2c_Stop();
        return FAILED;
    }
    I2c_SendByte(reg);
    I2c_WaitAck();
    I2c_SendByte(data);
    I2c_WaitAck();
    I2c_Stop();
    return SUCCESS;
}

/* 读设备寄存器定长数据 */
int8_t IIC_read_Bytes(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len)
{
    if (I2c_Start() == FAILED)
        return FAILED;
    I2c_SendByte(addr);
    if (I2c_WaitAck() == FAILED)
    {
        I2c_Stop();
        return FAILED;
    }
    I2c_SendByte(reg);
    I2c_WaitAck();
    I2c_Stop();
    I2c_Start();
    I2c_SendByte(addr + 1);
    if (I2c_WaitAck() == FAILED)
    {
        I2c_Stop();
        return FAILED;
    }
    while (len)
    {
        *data = I2c_ReadByte();
        if (len == 1)
            I2c_NoAck();
        else
            I2c_Ack();
        data++;
        len--;
    }
    I2c_Stop();
    return SUCCESS;
}

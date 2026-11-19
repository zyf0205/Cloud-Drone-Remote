/*******************************************************************
 * 软件模拟 I2C 总线驱动（PB6 = SCL，PB7 = SDA）
 *
 * 挂载器件：AT24C02 EEPROM（器件写地址 0xA0）
 * 所有权说明：
 *   调度器启动前用于上电自检与微调读取；
 *   调度器启动后仅由 Task_Storage 通过 eeprom.c 独占访问，无总线竞争。
 ******************************************************************/
#include "i2c.h"

#define I2C_OK  0
#define I2C_ERR 1

/* 软件时序微延时（适配 48MHz 主频下的标准/快速 I2C 速率） */
static void I2c_delay(void)
{
    volatile unsigned char i;
    for (i = 5; i > 0; i--)
    {
    }
}

/* 初始化 SCL/SDA 为开漏输出并预置总线空闲高电平 */
void IIC_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    /* 先置高电平再配置开漏模式，避免初始化瞬间拉低总线产生伪起始位 */
    SCL_H;
    SDA_H;

    GPIO_InitStructure.GPIO_Pin   = SCL_PIN | SDA_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_OD;
    GPIO_Init(IIC_GPIO, &GPIO_InitStructure);
}

/* 产生 I2C 起始信号：返回 I2C_ERR 表示总线被占用 */
static uint8_t I2c_Start(void)
{
    SDA_H;
    SCL_H;
    I2c_delay();
    if (!SDA_read)
        return I2C_ERR;
    SDA_L;
    I2c_delay();
    if (SDA_read)
        return I2C_ERR;
    SCL_L;
    I2c_delay();
    return I2C_OK;
}

/* 产生 I2C 停止信号 */
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

/* 主机发送应答 ACK */
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

/* 主机发送非应答 NACK */
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

/* 等待从机应答：返回 I2C_ERR 表示无应答 */
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
        return I2C_ERR;
    }
    SCL_L;
    return I2C_OK;
}

/* 发送 1 字节（MSB 先行） */
static void I2c_SendByte(uint8_t byte)
{
    uint8_t i = 8;

    while (i--)
    {
        SCL_L;
        I2c_delay();
        if (byte & 0x80u)
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

/* 读取 1 字节（MSB 先行） */
static uint8_t I2c_ReadByte(void)
{
    uint8_t i    = 8;
    uint8_t byte = 0;

    SDA_H; /* 释放 SDA 供从机驱动 */
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
            byte |= 0x01u;
    }
    SCL_L;
    return byte;
}

/* 起始条件 + 器件地址 + 寄存器地址，供读写两条路径共用。
 * 任一步无应答都会发停止条件收尾，避免总线被遗留在半途。 */
static int8_t I2c_BeginReg(uint8_t addr, uint8_t reg)
{
    if (I2c_Start() == I2C_ERR)
        return I2C_ERR;

    I2c_SendByte(addr);
    if (I2c_WaitAck() == I2C_ERR)
    {
        I2c_Stop();
        return I2C_ERR;
    }

    I2c_SendByte(reg);
    if (I2c_WaitAck() == I2C_ERR)
    {
        I2c_Stop();
        return I2C_ERR;
    }

    return I2C_OK;
}

/* 向设备指定寄存器连续写入定长字节 */
int8_t IIC_Write_Bytes(uint8_t addr, uint8_t reg, const uint8_t *data, uint8_t len)
{
    uint8_t i;

    if (I2c_BeginReg(addr, reg) == I2C_ERR)
        return I2C_ERR;

    for (i = 0; i < len; i++)
    {
        I2c_SendByte(data[i]);
        if (I2c_WaitAck() == I2C_ERR)
        {
            I2c_Stop();
            return I2C_ERR;
        }
    }

    I2c_Stop();
    return I2C_OK;
}

/* 向设备指定寄存器写入 1 字节 */
int8_t IIC_Write_One_Byte(uint8_t addr, uint8_t reg, uint8_t data)
{
    return IIC_Write_Bytes(addr, reg, &data, 1u);
}

/* 从设备指定寄存器连续读取定长字节 */
int8_t IIC_read_Bytes(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len)
{
    if (I2c_BeginReg(addr, reg) == I2C_ERR)
        return I2C_ERR;

    I2c_Stop();
    if (I2c_Start() == I2C_ERR)
        return I2C_ERR;

    I2c_SendByte((uint8_t)(addr + 1u));
    if (I2c_WaitAck() == I2C_ERR)
    {
        I2c_Stop();
        return I2C_ERR;
    }

    while (len > 0u)
    {
        *data = I2c_ReadByte();
        if (len == 1u)
            I2c_NoAck();
        else
            I2c_Ack();
        data++;
        len--;
    }

    I2c_Stop();
    return I2C_OK;
}

/*******************************************************************
 * AT24C02 EEPROM（软件 I2C，PB6/PB7，器件地址 0xA0）
 * 用于持久化保存摇杆微调值
 ******************************************************************/
#include "eeprom.h"
#include "delay.h"
#include "i2c.h"

#define AT24C02_ADDR 0xA0 // AT24C02 I2C 地址
#define SUCCESS      0
#define FAILED       1

/* 读 2 字节微调值 */
uint16_t read_AT24C02(uint8_t addr)
{
    uint16_t data;
    IIC_read_Bytes(AT24C02_ADDR, addr, (uint8_t *)&data, 2);
    return data;
}

/* 写 2 字节微调值 */
void write_AT24C02(uint8_t addr, uint16_t data)
{
    delay_ms(10); // 等待上一次写周期完成
    IIC_Write_Bytes(AT24C02_ADDR, addr, (uint8_t *)&data, 2);
}

/* EEPROM 自检：地址 0x55 写 0xAA 读回比对 */
uint8_t test_AT24C02(void)
{
    uint8_t test = 0;
    IIC_Write_One_Byte(AT24C02_ADDR, 0x55, 0xAA);
    delay_ms(20); // 等待内部写周期（AT24C02 最大 5ms）
    test = IIC_Read_One_Byte(AT24C02_ADDR, 0x55);
    if (test != 0xAA)
        return FAILED; // 存储异常
    else
        return SUCCESS;
}

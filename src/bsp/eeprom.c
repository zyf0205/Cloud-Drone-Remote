/*******************************************************************
 * AT24C02 EEPROM 板级驱动实现（软件 I2C，PB6/PB7，器件地址 0xA0）
 *
 * RTOS 使用约定：
 *  - 上电阶段（调度器未启动）：调用 test_AT24C02() 与 EEPROM_ReadI16()。
 *  - 运行阶段（调度器已启动）：仅由 Task_Storage 独占调用 EEPROM_WriteI16Raw()
 *    与 EEPROM_ReadI16()，写后等待使用 vTaskDelay(pdMS_TO_TICKS(EEPROM_WRITE_WAIT_MS))
 *    让出 CPU，彻底避免忙等延时阻塞无线发包与低功耗睡眠。
 ******************************************************************/
#include "eeprom.h"
#include "delay.h"
#include "i2c.h"

#define AT24C02_ADDR      0xA0u /* AT24C02 器件写地址 */
#define AT24C02_TEST_ADDR 0x55u /* 自检专用地址（避开微调区 0x00~0x07） */
#define AT24C02_TEST_VAL  0xAAu
#define EEPROM_OK         0u
#define EEPROM_ERR        1u

/* 读取 2 字节有符号微调值（返回 0=I2C成功，1=I2C失败） */
uint8_t EEPROM_ReadI16(uint8_t addr, int16_t *out_val)
{
    uint8_t buf[2] = {0u, 0u};

    if (IIC_read_Bytes(AT24C02_ADDR, addr, buf, 2u) != 0)
        return EEPROM_ERR;

    /* 小端还原，与旧版 (uint8_t *)&data 在 Cortex-M3 上的字节序完全一致 */
    *out_val = (int16_t)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8));
    return EEPROM_OK;
}

/* 原始写入 2 字节有符号微调值（不含写周期延时，调用方需等待 >= 5ms 后再读写） */
uint8_t EEPROM_WriteI16Raw(uint8_t addr, int16_t val)
{
    uint16_t raw = (uint16_t)val;
    uint8_t buf[2];

    buf[0] = (uint8_t)(raw & 0xFFu);
    buf[1] = (uint8_t)((raw >> 8) & 0xFFu);

    return (IIC_Write_Bytes(AT24C02_ADDR, addr, buf, 2u) == 0) ? EEPROM_OK : EEPROM_ERR;
}

/* 上电自检：向测试地址写 0xAA 等待写周期后读回比对（返回 0=正常，1=异常） */
uint8_t test_AT24C02(void)
{
    uint8_t readback = 0u;

    if (IIC_Write_One_Byte(AT24C02_ADDR, AT24C02_TEST_ADDR, AT24C02_TEST_VAL) != 0)
        return EEPROM_ERR;

    delay_ms(20u); /* 上电阶段尚未启动调度器，使用 TIM3 精确毫秒延时 */

    if (IIC_read_Bytes(AT24C02_ADDR, AT24C02_TEST_ADDR, &readback, 1u) != 0)
        return EEPROM_ERR;

    return (readback == AT24C02_TEST_VAL) ? EEPROM_OK : EEPROM_ERR;
}

/*******************************************************************
 * AT24C02 EEPROM 板级驱动头文件（微调参数持久化）
 *
 * 地址布局（与旧版固件保持 100% 兼容，升级不丢失已存微调）：
 *   0x00 ~ 0x01 : Roll 微调值（int16_t，小端 2 字节）
 *   0x04 ~ 0x05 : Pitch 微调值（int16_t，小端 2 字节）
 *   0x55        : 上电自检测试字节（写 0xAA 读回校验）
 ******************************************************************/
#ifndef _AT24C02_H_
#define _AT24C02_H_

#include <stdint.h>

#define ROLL_ADDR_OFFSET     0u  /* Roll 微调值存储地址 */
#define PITCH_ADDR_OFFSET    4u  /* Pitch 微调值存储地址 */
#define EEPROM_WRITE_WAIT_MS 10u /* AT24C02 页写内部周期等待（手册最大 5ms，留 2 倍裕量） */

extern uint8_t test_AT24C02(void);
extern uint8_t EEPROM_ReadI16(uint8_t addr, int16_t *out_val);
extern uint8_t EEPROM_WriteI16Raw(uint8_t addr, int16_t val);

#endif /* _AT24C02_H_ */

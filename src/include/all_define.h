/*******************************************************************
 * 项目顶层公共汇总头文件
 *
 * 说明：底层驱动与纯算法模块建议按需包含最小依赖头，
 * 应用层与启动入口可通过本文件统一引入系统基础接口。
 ******************************************************************/
#ifndef __ALL_DEFINE_H__
#define __ALL_DEFINE_H__

#include "stm32f10x.h"
#include "all_data.h"
#include "sys.h"
#include "delay.h"
#include "init.h"
#include "tim.h"
#include "adc.h"
#include "i2c.h"
#include "spi.h"
#include "buzzer.h"
#include "key.h"
#include "nrf24l01.h"
#include "eeprom.h"
#include "rc_protocol.h"
#include "remote.h"

/* 统一函数状态返回码：0 表示成功，1 表示失败 */
#undef SUCCESS
#define SUCCESS 0
#undef FAILED
#define FAILED 1

#endif /* __ALL_DEFINE_H__ */

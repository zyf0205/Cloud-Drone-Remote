#ifndef _DELAY_H
#define _DELAY_H

#include "stm32f10x.h"

extern void cycleCounterInit(void);
extern void delay_ms(uint16_t nms);

#endif

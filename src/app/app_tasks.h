#ifndef __APP_TASKS_H
#define __APP_TASKS_H

#include "FreeRTOS.h"
#include "task.h"

#define TASK_PRIO_RC 3

extern void RCTask(void *param);

#endif

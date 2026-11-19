/*******************************************************************
 * 蜂鸣器板级驱动头文件（PA12 推挽输出，高电平发声）
 *
 * 所有权说明：
 *   调度器启动后仅允许 Task_Buzzer 调用 Buzzer_On/Off，
 *   其他业务任务须通过 App_PostBuzzerEvent() 投递音效事件。
 ******************************************************************/
#ifndef __BUZZER_H
#define __BUZZER_H

extern void Buzzer_Init(void);
extern void Buzzer_On(void);
extern void Buzzer_Off(void);

#endif /* __BUZZER_H */

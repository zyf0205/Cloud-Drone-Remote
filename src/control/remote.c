/*******************************************************************
 *@brief 
 *@brief 
 *@time ZIN���Ӳ�Ʒ�뼼�� 2017.1.8
 *@editorС��&zin
 *�ɿذ���QQȺ551883670,����759421287@qq.com
 *����Ȩʹ����Ա����ֹʹ�á���ֹ���ģ�Υ��һ�����֣���Ȩ������
 *רҵ�ķɿز�����õķɿ�
 ******************************************************************/
#include "stm32f10x.h"
#include "remote.h"
#include "adc.h"
#include "sys.h"
#include "nrf24l01.h"
#include <math.h>
#include "eeprom.h"
#include "delay.h"
__align(4) _st_Remote Remote={1000,1000,1000,1000,1000,1000,1000,1000};


void key(void);
	

struct //У׼����
{
	int16_t flag;		//У׼��־λ
	int16_t roll;
	int16_t pitch;	
	int16_t thr;	

	int16_t yaw;	
}offset = {0};
/******************************************************************************
����ԭ�ͣ�
��    �ܣ�	������ʼ��
*******************************************************************************/ 

//PA15 AUX1
//PB11 AUX2
//PC14 AUX3
//PC15 AUX4
//PB1 front
//PB0 back
//PA6 left
//PA7 right
void key_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	
	AFIO->MAPR = 0X02000000; //ʹ������SWD
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC , ENABLE);
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6|GPIO_Pin_7|GPIO_Pin_4|GPIO_Pin_15;      
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;    //��������
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_11;
	GPIO_Init(GPIOB, &GPIO_InitStructure);	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14|GPIO_Pin_15;
	GPIO_Init(GPIOC, &GPIO_InitStructure);	
}
/******************************************************************************
����ԭ�ͣ�
��    �ܣ�	У׼���ݳ�ʼ��
*******************************************************************************/ 
void RC_INIT(void)
{
	offset.roll =	read_AT24C02(ROLL_ADDR_OFFSET);//��ȡУ׼ֵ
	offset.pitch =	read_AT24C02(PITCH_ADDR_OFFSET);
//	write_AT24C02(ROLL_ADDR_OFFSET,0);
//	write_AT24C02(PITCH_ADDR_OFFSET,0);
	key_init();
}
/******************************************************************************
����ԭ�ͣ�
��    �ܣ�	10ms����һ�����ݣ�ÿ��ͨ�����Ǹ�λ��ǰ��λ�ں�
*******************************************************************************/ 
uint8_t tx_data[32] = {0xAA,0xAF,0x03,32-5};//����ͨ��Э��
void NRF_SEND(void)
{
	for(uint8_t i=4;i<26;i+=2)
	{
		tx_data[i] = *((uint8_t*)&Remote+i-3);	 //��λ��ǰ
		tx_data[31] += tx_data[i];
		tx_data[i+1] = *((uint8_t*)&Remote+i-4);	
		tx_data[31] += tx_data[i];
	}
	tx_data[31] = 0;
		for(uint8_t i=0;i<31;i++)  //У��λ
		{
			tx_data[31] +=  tx_data[i];
		}
	NRF24L01_TxPacket((uint8_t*)&tx_data); //����NRF��������
}
/******************************************************************************
����ԭ�ͣ�
��    �ܣ�	ң�����ݽ���
*******************************************************************************/ 
void RC_Analy(void) //10ms����һ��
{
	static uint16_t last_thr,last_roll,last_pitch,last_yaw;
	//ADC_ConvertedValue  DMA�Զ��ض���ֵ��ֱ�ӿ����ã������ֶ���ȡADCת�����
	Remote.thr = (uint16_t)(0.20f*ADC_ConvertedValue[1])+1000 + offset.thr;//����
	last_thr = Remote.thr = Remote.thr*0.25f + 0.75f*last_thr;
	Remote.pitch 	= 	1000 + (uint16_t)(0.25f*ADC_ConvertedValue[2]) + offset.pitch;//
	last_pitch = Remote.pitch = Remote.pitch*0.25f + 0.75f*last_pitch;
	Remote.yaw 		= 	1000 + (uint16_t)(0.25f*ADC_ConvertedValue[0]) + offset.yaw;//����
	last_yaw = Remote.yaw = Remote.yaw*0.25f + 0.75f*last_yaw;
	Remote.roll 	=	1000 +(uint16_t)(0.25f*ADC_ConvertedValue[3]) + offset.roll;//����
	last_roll = Remote.roll = Remote.roll*0.25f + 0.75f*last_roll;
  if(Remote.thr>2000)
			Remote.thr = 2000;
	else if(Remote.thr<1000)
			Remote.thr = 1000;
  if(Remote.pitch>2000)
			Remote.pitch = 2000;
	else if(Remote.pitch<1000)
			Remote.pitch = 1000;
  if(Remote.roll>2000)
			Remote.roll = 2000;
	else if(Remote.roll<1000)
			Remote.roll = 1000;
  if(Remote.yaw>2000)
			Remote.yaw = 2000;
	else if(Remote.yaw<1000)
			Remote.yaw = 1000;
	
	key();//ɨ�谴��
	NRF_SEND();//���͵��ɿض�
}


/******************************************************************************
����ԭ�ͣ�
��    �ܣ�	����ɨ��
*******************************************************************************/ 
void key(void)
{
	
//PA15 AUX1
//PB11 AUX2
//PC14 AUX3
//PC15 AUX4
//PB1 front
//PB0 back
//PA6 left
//PA7 right	
	
#define aux1 GPIO_Pin_15 //GPIOA
#define aux2 GPIO_Pin_11 //GPIOB
#define aux3 GPIO_Pin_14//GPIOC
#define aux4 GPIO_Pin_15//GPIOC
#define front GPIO_Pin_0 //GPIOB
#define back GPIO_Pin_1 //GPIOB
#define left GPIO_Pin_6 //GPIOA
#define right GPIO_Pin_7 //GPIOA	
	
   volatile static uint8_t status = 0;	
 	static uint32_t temp;
	switch(status)
	{
		case 0:
			if(SysTick_count - temp >30) //300ms ������
			{
				if(  ((GPIOA->IDR & (GPIO_Pin_6|GPIO_Pin_7|GPIO_Pin_15)) == (GPIO_Pin_6|GPIO_Pin_7|GPIO_Pin_15))
					&& ((GPIOB->IDR & (GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_11)) == (GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_11))
					&& ((GPIOC->IDR & (GPIO_Pin_14|GPIO_Pin_15)) == (GPIO_Pin_14|GPIO_Pin_15))
					)
					status = 1;
				BEEP_L;
			}
			break;
		case 1:
			if(((GPIOA->IDR & (GPIO_Pin_6|GPIO_Pin_7|GPIO_Pin_15)) != (GPIO_Pin_6|GPIO_Pin_7|GPIO_Pin_15))
				|| ((GPIOB->IDR & (GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_11)) != (GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_11))
				|| ((GPIOC->IDR & (GPIO_Pin_14|GPIO_Pin_15)) != (GPIO_Pin_14|GPIO_Pin_15))
				)
				status = 2;
			break;
		case 2:	
			if(!(GPIOA->IDR & aux1))
			{
					Remote.AUX1 ^= (2000^1000); //1000��2000֮��仯
			}
			else if(!(GPIOB->IDR & aux2))
			{
					Remote.AUX2 ^= (2000^1000); //1000��2000֮��仯
			}
			else if(!(GPIOC->IDR & aux3))
			{
					Remote.AUX3 ^= (2000^1000); //1000��2000֮��仯
			}
			else if(!(GPIOC->IDR & aux4))
			{
					Remote.AUX4 ^= (2000^1000); //1000��2000֮��仯	
			}			
			else if(!(GPIOA->IDR & left))
			{
				if(offset.roll>-250)
				{
					offset.roll-=5;	
					write_AT24C02(ROLL_ADDR_OFFSET,offset.roll);	//��¼΢��У׼ֵ	
				}					
			}
			else if(!(GPIOA->IDR & right))
			{
				if(offset.roll<250)
				{
					offset.roll+=5;
					write_AT24C02(ROLL_ADDR_OFFSET,offset.roll);
				}					
			}
			else if(!(GPIOB->IDR & front))
			{
				if(offset.pitch>-250)
				{
					offset.pitch-=5;	
					write_AT24C02(PITCH_ADDR_OFFSET,offset.pitch);	//��¼΢��У׼ֵ	
				}
			}
			else if(!(GPIOB->IDR & back))
			{
				if(offset.pitch<250)
				{
					offset.pitch+=5;
					write_AT24C02(PITCH_ADDR_OFFSET,offset.pitch);	//��¼΢��У׼ֵ	
				}
			}
			status = 0;			
			BEEP_H;
			temp = SysTick_count;
			break;
		}			
}



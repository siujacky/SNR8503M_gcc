/*******************************************************************************
 * ��Ȩ���� (C)2021, SNANER SEMICONDUCTOR Co.ltd
 *
 * �ļ����ƣ� SNR85_03x_flash.h
 * �ļ���ʶ��
 * ����ժҪ�� flash������������
 * ����˵���� ��
 * ��ǰ�汾�� V 1.0
 * ��    �ߣ�   Li
 * ������ڣ� 2021��10��14��
 *
 *
 *******************************************************************************/
#ifndef __snr8503x_FLASH__
#define __snr8503x_FLASH__
#include "snr8503x.h"

extern volatile u32 erase_flag;
extern volatile u32 progm_flag;
void EraseSector(u32 adr);
int ProgramPage (unsigned long adr, unsigned long sz, unsigned char *buf);  /* FIX-012: header was 'void'; impl returns int (1=ok, 0=fail) */
void Read_More_Flash(u32 adr, u32 sz, u32 *buf);

#endif

/************************ (C) COPYRIGHT SNANER SEMICONDUCTOR *****END OF FILE****/

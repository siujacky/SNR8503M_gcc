 /**
 * @file 
 * @copyright (C)2015, SNANER SEMICONDUCTOR Co.ltd
 * @brief �ļ����ƣ� SNR8503x_dac.c\n
 * �ļ���ʶ�� �� \n
 * ����ժҪ�� DAC������������ \n
 * ����˵���� �� \n
 *@par �޸���־:
 * <table>
 * <tr><th>Date	        <th>Version  <th>Author  <th>Description
 * <tr><td>2022��4��18�� <td>1.0     <td>Li      <td>����
 * </table>
 */

#include "snr8503x_dac.h"
#include "string.h"
#include "hardware_config.h"

DAC_CheckTypeDef Stru_DAC_CHECK;

 /**
 *@brief @b ��������:   void DAC_StructInit(DAC_InitTypeDef* DAC_InitStruct)
 *@brief @b ��������:   DAC�ṹ���ʼ��
 *@see�����ú�����       ��
 *@param���������      ADC_InitTypeDef
 *@param���������      ��
 *@return�� �� ֵ��     ��
 *@note����˵����       ��
 *@warning             ��   
 *@par ʾ�����룺
 *@code    
           DAC_StructInit DAC_InitStructure;
		   DAC_StructInit(&DAC_InitStructure); //��ʼ���ṹ��
  @endcode    
 *@par �޸���־:
 * <table>
 * <tr><th>Date	        <th>Version  <th>Author  <th>Description
 * <tr><td>2022��4��18�� <td>1.0     <td>Li      <td>����
 * </table>
 */
void DAC_StructInit(DAC_InitTypeDef* DAC_InitStruct)
{
    memset(DAC_InitStruct, 0, sizeof(DAC_InitTypeDef));
}

/**
 *@brief @b ��������:   void DAC_init(DAC_InitTypeDef* DAC_InitStruct)
 *@brief @b ��������:   DAC��ʼ������
 *@see�����ú�����      SYS_AnalogModuleClockCmd()
 *@param���������      DAC_InitTypeDef
 *@param���������      ��
 *@return�� �� ֵ��     ��
 *@note����˵����       ��
 *@warning             ��
 *@par ʾ�����룺
 *@code    
			  DAC_InitTypeDef DAC_InitStructure;
			  DAC_StructInit(&DAC_InitStructure);
			  DAC_InitStructure.DACOUT_EN = DISABLE ;    // DAC�����IO��
			  DAC_InitStructure.DAC_GAIN = DAC_RANGE_3V0��//ADC����ѡ��3.0V��ֻ��B�汾оƬ֧��4.8V����
			  DAC_Init(&DAC_InitStructure);              // DAC��ʼ��
  @endcode   
 *@par �޸���־:   
 * <table>
 * <tr><th>Date	        <th>Version    <th>Author      <th>Description
 * <tr><td>2022��4��18�� <td>1.0     <td>Li      <td>����
 * </table>
 */
void DAC_Init(DAC_InitTypeDef* DAC_InitStruct)
{
    SYS_AnalogModuleClockCmd(SYS_AnalogModule_DAC, ENABLE);  /* DAC ʱ��ʹ�� */
	
	  DAC_InitStruct->DAC_VERSION = REG32(0x40000004); /* ��ȡ03оƬ�汾�� 1 ΪA�汾��2ΪB�汾 */
	
	  SYS_WR_PROTECT = 0x7a83;  /* �����Ĵ���д���� */
	
	  SYS_AFE_REG1 |= (DAC_InitStruct->DACOUT_EN << 1);/* DAC�����IO��ʹ������ */
	  
	  SYS_WR_PROTECT = 0xffff;  /* �����Ĵ���д���� */
	
		/* Uncalibrated identity defaults — formula is (input * AMC) >> 9 + DC,
		 * so AMC=512 cancels the shift and DC=0 means no offset. Compensate
		 * for per-chip variation by adjusting SHORT_CURRENT_DAC in MC_Parameter.h
		 * after bench-measuring the comparator trip point. */
		if ((DAC_InitStruct->DAC_VERSION) == 1)
		{
			Stru_DAC_CHECK.VersionAndDACGain = 1;
			Stru_DAC_CHECK.DAC_AMC = 512;
			Stru_DAC_CHECK.DAC_DC  = 0;
		}
		else if (DAC_InitStruct->DAC_VERSION == 2)
		{
			if(DAC_InitStruct->DAC_GAIN == DAC_RANGE_3V0)
			{
				Stru_DAC_CHECK.VersionAndDACGain = 2;
				Stru_DAC_CHECK.DAC_AMC = 512;
				Stru_DAC_CHECK.DAC_DC  = 0;
			}
			else if(DAC_InitStruct->DAC_GAIN == DAC_RANGE_4V8)
			{
				Stru_DAC_CHECK.VersionAndDACGain = 3;
				Stru_DAC_CHECK.DAC_AMC = 512;
				Stru_DAC_CHECK.DAC_DC  = 0;
			}
		}
}

 /**
 *@brief @b ��������:   void DAC_OutputValue(uint32_t DACValue)
 *@brief @b ��������:   DAC�����������ֵ����
 *@see�����ú�����       ��
 *@param���������       DACValue:DAC�����ѹ��Ӧ������
 *@param���������       ��
 *@return�� �� ֵ��      ��
 *@note����˵����        ��
 *@warning              ��   
 *@par ʾ�����룺
 *@code    
           DAC_OutputValue(128);//DAC���128*3.0/256 = 1.5V
  @endcode    
 *@par �޸���־:
 * <table>
 * <tr><th>Date	        <th>Version  <th>Author  <th>Description
 * <tr><td>2022��4��18�� <td>1.0     <td>Li      <td>����
 * </table>
 */

void DAC_OutputValue(uint32_t DACValue)
{
	  s16 temp1 = 0;
	

	  if(DACValue >=255) /* �޷� */
		{
		    DACValue = 255;
		}
			temp1 = (s16)(DACValue * Stru_DAC_CHECK.DAC_AMC >> 9 )+ (s16)Stru_DAC_CHECK.DAC_DC;
		if(temp1 < 0)
		{
		  temp1 = 0;		
		  }
		  else if(temp1 > 255)
		{
		  temp1 = 255;
		}	
	  SYS_AFE_DAC = (u32)temp1;
}
/**
 *@brief @b ��������:   void DAC_OutputVoltage(uint32_t DACVoltage)
 *@brief @b ��������:   DAC���ģ������ֵ����
 *@see�����ú�����       ��
 *@param���������       DACVoltageΪQ12��ʽ,��Χ0~4096��ӦDAC���0~3V
 *@param���������       ��
 *@return�� �� ֵ��      ��
 *@note����˵����        03xDAC����Ϊ�̶���3.0V  8λ
                        ��05x��08x��ͬ��DAC��������Ҫ��Ϊ������ٸ�ֵ���Ĵ���SYS_AFE_DAC
                        y = A * x + B����DAC�����ѹ = DAC_Gain * x + DAC_Offset;
                        У��ֵ�ӵ�ַ��ֱ�Ӷ�ȡ �ֱ��Ӧ0x000001c4 �� 0x000001c0;
                        ����Read_Trim();��nvr.o�С�
 *@warning              ��   
 *@par ʾ�����룺
 *@code    
           DAC_OutputVoltage(BIT12 * 0.6);//DAC���0.6V
  @endcode    
 *@par �޸���־:
 * <table>
 * <tr><th>Date	        <th>Version  <th>Author  <th>Description
 * <tr><td>2022��4��18�� <td>1.0     <td>Li      <td>����
 * </table>
 */
void DAC_OutputVoltage(uint16_t DACVoltage)
{
	  s32 temp = 0;
	  u32 range = 0;
	  s16 temp1 = 0; 
    
	  if(Stru_DAC_CHECK.VersionAndDACGain <= 2)
		{
		    range = (uint16_t)((1.0/3.0)*BIT8); /* BIT8 ��ʾ2^8 DACΪ8λ */
		}
    else if(Stru_DAC_CHECK.VersionAndDACGain == 3)
		{
		    range = (uint16_t)((1.0/4.8)*BIT8); /* BIT8 ��ʾ2^8 DACΪ8λ */
		}
	  temp = (DACVoltage * range ) >> 12; 
	
	  if(temp >=255) /* �޷� */
		{
		    temp = 255;
		}
		
		temp1 = (s16)((temp * Stru_DAC_CHECK.DAC_AMC )>> 9) + (s16)Stru_DAC_CHECK.DAC_DC; ;
		
		if(temp1 < 0)
		{
		  temp1 = 0;
		}
		else if(temp1 > 255)
		{
		  temp1 = 255;
		}		
	  SYS_AFE_DAC = temp1;
	
}

/************************ (C) COPYRIGHT SNANER SEMICONDUCTOR *****END OF FILE****/

/*******************************************************************************
 * �ļ����ƣ� interrupt.c
 * �ļ���ʶ��
 * ����ժҪ�� �жϷ�������ļ�
 * ����˵���� ��
 * ��ǰ�汾�� V 1.0
 * ��    �ߣ� Li
 * ������ڣ� 2021��11��15��
 *
 *******************************************************************************/
#include "hardware_config.h"
#include "MC_Parameter.h"
#include "MC_Drive.h"
#include "motor_structure.h"
#include "M1_StateMachine.h"
#include "MosTest.h"
#include "UR_Ctrl.h"
#include "timebase.h"
#include "hall_accumulator.h"  /* phase 5.2 */
#include "motor_mode.h"        /* Phase 13 */
#if MODULE_MOTOR_SENSORLESS
#include "sensorless_adapter.h"
#endif

void Digital_Value_Getting(void);
/*******************************************************************************
 �������ƣ�    void Digital_Value_Getting(void)
 ����������    ADCֵ��ȡ
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2022/5/26      V1.0           Li
 *******************************************************************************/
void Digital_Value_Getting(void)
{
		/* ADCֵ��ȡ */	
		User_sys.BLDC_SensorlessCtr.peakBusCurrent = (s16)(ADC_DAT0 - User_sys.BLDC_SensorlessCtr.PeakCurrOffset); //����ֵ��ȡ  
			
		if(ADC_DAT1 & BIT15)		/* VBus */
		{
		  User_sys.BLDC_SensorlessCtr.nBUS_Vol_ADC = 0;
		}
    else
		{
      User_sys.BLDC_SensorlessCtr.nBUS_Vol_ADC = (User_sys.BLDC_SensorlessCtr.nBUS_Vol_ADC + ADC_DAT1)/2;		
		}		
		
		if(ADC_DAT2 & BIT15)		/* Speed command */
		{
			User_sys.BLDC_SensorlessCtr.nSpeed_ADC = 0;
		}
		else
		{
			User_sys.BLDC_SensorlessCtr.nSpeed_ADC = (ADC_DAT2 >> 4);	
		}
		
		if(ADC_DAT3 & BIT15)		/* MOS NTC */
		{
		  User_sys.BLDC_SensorlessCtr.nMOS_NTC_ADC = 0;
		}
		else
		{
		  User_sys.BLDC_SensorlessCtr.nMOS_NTC_ADC = (User_sys.BLDC_SensorlessCtr.nMOS_NTC_ADC + ADC_DAT3)/2;
		}		
}

/*******************************************************************************
 �������ƣ�    void ADC_IRQHandler(void)
 ����������    ADC0�жϴ�������
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2021/11/15      V1.0        Li        ����
 *******************************************************************************/
void ADC_IRQHandler(void)
{
    ADC_IF |= BIT1 | BIT0;

    User_sys.BLDC_SensorlessCtr.nSys_TimerPWM++;        /* PWMʱ�������� */
    timebase_pwm_tick();                                 /* phase 3.2: ms counter for protocol */

		/* ADCֵ��ȡ */	
		Digital_Value_Getting();	
	
		if(MOS_Selftest.MosTest_start == 1)
		{
		  MOS_Selftest.PWM_cnt++;
			MOS_Selftest.PWM_delay++;
		}  
}


/*******************************************************************************
 �������ƣ�    void HALL_IRQHandler(void)
 ����������    HALL�жϴ�������
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2021/11/15      V1.0        Li        ����
 *******************************************************************************/
void HALL_IRQHandler(void)
{
#if MODULE_MOTOR_HALL
	static unsigned char i = 0;
	static unsigned char j = 0;

	static unsigned char hall_index = 0;
	
	if (HALL_GetIRQFlag(HALL_CAPTURE_EVENT))  /* HALL�źű仯�����¼� */
	{
    HALL_ClearIRQFlag(HALL_CAPTURE_EVENT);	/* ���HALL�����жϱ�־λ */
		
		hall_info = HALL_GetFilterValue();    					/* ȡ��HALLֵ���˲���� */
		hall_cnt = HALL_WIDCount();		      						/* ȡ��AHLL��������ʱHALL������ֵ */
		Motor.BLDC.u8HallValueRenew = hall_info;	/* �洢hall�ź� */
#if MODULE_MOTOR_HALL
		hall_accum_tick(Motor.BLDC.u8Direction);    /* phase 5.2: feed Hall counter */
#endif
		
		if(i >= 5){	i = 0;	}
		else			{	i++;		}		
		Motor.Control.u32MotorElePhaseValue[i] = hall_cnt;
		
		if((Motor.BLDC.u8FlagMotorIsRunning == 1) && (Motor.Control.u8Read_Hall_finish_flag == 0))
		{			
			// ��ʵ�ʻ����任ʱ��
			if((Motor.BLDC.u8HallValueRenew == 5) || (Motor.BLDC.u8HallValueRenew == 0))	// 120�� �� 60��
			{
				hall_index = 0;
				Read_hall_tab[hall_index] = Motor.BLDC.u8HallValueRenew;
				hall_index = 1;
			}
			else
			{
				Read_hall_tab[hall_index] = Motor.BLDC.u8HallValueRenew;
				if(hall_index < 5)
				{
					hall_index++;
				}
				else
				{
					hall_index = 0;
				}
			}
		}
		
		// ����������ڻ�ȡת������
		Motor.Control.u32MotorElePeriodValue = 0;
		for(j=0;j<6;j++)
		{
			Motor.Control.u32MotorElePeriodValue += Motor.Control.u32MotorElePhaseValue[j];				// ��������
		}
		Motor.Control.u32MotorElePeriodValueTemp = Motor.Control.u32MotorElePeriodValue;

		// ��ת, �����ź�ȡ��
		if(Motor.BLDC.u8Direction == CCW)
		{
			Motor.BLDC.u8HallValueRenew = (0x07) & (~Motor.BLDC.u8HallValueRenew);
		}
		
		if(Motor.BLDC.u8FlagMotorIsRunning == 1)//
		{
			// �ж�hall�ź��Ƿ���ȷ, �����л��ദ��
			BLDC_Sensor_Judge(Motor.BLDC.u8HallValueRenew);
		
			if(Motor.Control.u8Read_Hall_finish_flag == 0)
			{
				if(Motor.Control.u16Phase_Compensation_cnt > 30)
				{
					#if (Hall_Degree_60_or_120 == Degree60)
						if((Read_hall_tab[1] == 7) && (Read_hall_tab[5] == 1))	// CW
						{
							Motor.Control.u8Read_Hall_CWCCW_flag = CW;
							Motor.Control.u8Read_Hall_finish_flag = 1;
						}
						else if((Read_hall_tab[1] == 1) && (Read_hall_tab[5] == 7))	// CCW
						{
							Motor.Control.u8Read_Hall_CWCCW_flag = CCW;
							Motor.Control.u8Read_Hall_finish_flag = 1;
						}
						else		// ��������, ���ܽ��г�ǰ����
						{
							Motor.Control.u16Phase_Compensation_cnt = 0;
							Motor.Control.u8Read_Hall_finish_flag = 0;
						}					
					#else
						if((Read_hall_tab[1] == 4) && (Read_hall_tab[5] == 1))	// CW
						{
							Motor.Control.u8Read_Hall_CWCCW_flag = CW;
							Motor.Control.u8Read_Hall_finish_flag = 1;
						}
						else if((Read_hall_tab[1] == 1) && (Read_hall_tab[5] == 4))	// CCW
						{
							Motor.Control.u8Read_Hall_CWCCW_flag = CCW;
							Motor.Control.u8Read_Hall_finish_flag = 1;
						}
						else		// ��������, ���ܽ��г�ǰ����
						{
							Motor.Control.u16Phase_Compensation_cnt = 0;
							Motor.Control.u8Read_Hall_finish_flag = 0;
						}
					#endif
				}
			}
		}
	
		if(Motor.Control.u16Phase_Compensation_cnt > 50)	// �ȴ�ת�������ȶ�
		{
			// ���ó�ǰ���ദ�� 
			#if EN_HALL_PHASE_COMPENSATION
				if(Motor.Control.u8Cal_Hall_Angle_OK_flag == 1)
				{				
					// ʹ��Ӳ������������
//					HallAngle_Division.Dividend = Motor.Control.u32Cal_Hall_Angle_Act_Cmd * Motor.Control.u32MotorElePeriodValueTemp;		// ������
//					HallAngle_Division.Divisor = 360;		// ����, һ����������360��
//					Motor.Control.i32_AdvanceAngleValue = DSP_CalcDivision(&HallAngle_Division);
					Motor.Control.i32_AdvanceAngleValue = Motor.Control.u32Cal_Hall_Angle_Act_Cmd * Motor.Control.u32MotorElePeriodValueTemp / 360;
					
					TIM_Timer_Set_TH_Value(TIMER1, Motor.Control.i32_AdvanceAngleValue);
					TIM_Timer_ClearCnt(TIMER1);					// ���TIM1����ֵ
					TIM_TimerCmd(TIMER1, ENABLE); 			// ����ʱ����л���
				}
			#endif
			/*--------------- ����Ϊ��ǰ�������� ----------------*/
		}
	
		Motor.BLDC.u32Freerun_Det_cnt = 0;				/* ˳������ʱ���� */
		if((Motor.BLDC.u8FlagEnMotorRun == 1) && (e1M1_RunSubState == M1_RunState_Freerun_Det))
    {
			Motor.BLDC.u8FlagMotorIsRunning = 1;
			
			e1M1_RunSubState = M1_RunState_Spin;
		
			// ��ȡHall�ź�
			hall_info = HALL_GetFilterValue();    /* ȡ��HALLֵ���˲����*/
			Motor.BLDC.u8HallValueRenew = hall_info;
			// ��������ת, ��ȡ��һ��hall�ź�
			if(Motor.BLDC.u8Direction == CCW)
			{
				Motor.BLDC.u8HallValueRenew = (0x07) & (~Motor.BLDC.u8HallValueRenew);
			}
			// ����hall�ź�
			Motor.BLDC.u8HallValueOld = Motor.BLDC.u8HallValueRenew;
			
			BLDC_Sensor_control(Motor.BLDC.u8HallValueRenew);
			MCPWM_UPDATE = 0;
			MCPWM_TH00 = 	- MIN_PWM_DUTY;
			MCPWM_TH01 = 		MIN_PWM_DUTY;
			MCPWM_TH10 = 	-	MIN_PWM_DUTY;
			MCPWM_TH11 = 		MIN_PWM_DUTY;
			MCPWM_TH20 = 	-	MIN_PWM_DUTY;
			MCPWM_TH21 = 		MIN_PWM_DUTY;
			MCPWM_UPDATE = 0xff;
			PWMOutputs(MCPWM0,ENABLE);
			
			Motor.Control.u32MCPWM_TH_temp = MIN_PWM_DUTY;
		}
	}
	
	// ��ת���
	if(HALL_GetIRQFlag(HALL_OVERFLOW_EVENT)) 		/* HALL����������¼�*/
	{
    HALL_ClearIRQFlag(HALL_OVERFLOW_EVENT);		/* ���HALL����жϱ�־λ*/
		
		if(Motor.BLDC.u8FlagMotorIsRunning == 1)
		{
			Motor.Control.u16MotorShakeDelayProtectCnt++;
		}
	}
#endif /* MODULE_MOTOR_HALL */
}

/*******************************************************************************
 �������ƣ�    void TIMER1_IRQHandler(void)
 ����������    TIMER1�жϴ�������
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 *******************************************************************************/
void TIMER1_IRQHandler(void)
{
	if(UTIMER1_IF & TIM_IRQ_IF_ZC)   /* UTIMER1�����ж� */
	{
		UTIMER1_IF = TIM_IRQ_IF_ZC;    /* ����жϱ�־λ */
	}

	#if EN_HALL_PHASE_COMPENSATION
			if(Motor.Control.u8Read_Hall_CWCCW_flag == CW)
			{
				Motor.BLDC.u8HallValueNext = Motor_step_tab_cw[Motor.BLDC.u8HallValueOld];
			}
			else
			{			
				Motor.BLDC.u8HallValueNext = Motor_step_tab_ccw[Motor.BLDC.u8HallValueOld];
			}
	
		// ��ǰ����
		BLDC_Sensor_control(Motor.BLDC.u8HallValueNext);
		Motor.Control.u8PhaseChange_ON_COMPENSATION_flag = 1;

		TIM_TimerCmd(TIMER1, DISABLE); 
	#endif
}

/*******************************************************************************
 �������ƣ�    void TIMER0_IRQHandler(void)
 ����������    TIMER0�жϴ�������
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 *******************************************************************************/
void TIMER0_IRQHandler(void)
{
	 if(UTIMER0_IF & TIM_IRQ_IF_ZC)   /* UTIMER0�����ж�*/
	 {
			UTIMER0_IF = TIM_IRQ_IF_ZC;    /* ����жϱ�־λ*/
				 
			/* ʱ��500us */
			User_sys.struTaskScheduler.bTimeCnt1ms++;
			User_sys.struTaskScheduler.nTimeCnt10ms ++;
			User_sys.struTaskScheduler.nTimeCnt100ms ++;
			User_sys.struTaskScheduler.nTimeCnt500ms++;

#if MODULE_MOTOR_HALL
			ICP_Loader();    /* Hall-lib speed-from-period processing */
#endif
	 }
}

/*******************************************************************************
 �������ƣ�    void CMP_IRQHandler(void)
 ����������    �Ƚ����жϴ�������
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2021/11/15      V1.0        Li        ����
 *******************************************************************************/
void CMP_IRQHandler(void)
{
	volatile u8 t_i;
	u8 CMP_fail_cnt = 0;
  //CMP_IF = BIT0 | BIT1;

#if MODULE_MOTOR_SENSORLESS
	/* Phase 13: BEMF zero-crossing on CMP0. Routed to the closed-lib
	 * COMP_Motor_Zero_Detect when in sensorless mode. */
	if ((CMP_IF & BIT0) && g_motor_mode == MOTOR_MODE_SENSORLESS) {
		CMP_IF = BIT0;
		Motor_1M1st.BLDC_CMP.u32COMP_OUT_temp = CMP0_OUT_LEVEL();
		CMP_ZeroCross_lib.u8CMP_Zero_OUT_Value = Motor_1M1st.BLDC_CMP.u32COMP_OUT_temp >> 8;
		COMP_Motor_Zero_Detect(&CMP_ZeroCross_lib);
	}
#endif

	if(CMP_IF & BIT1)
	{
		CMP_IF = BIT1;
		CMP_fail_cnt = 0;
		for(t_i = 0; t_i < 4; t_i++)
		{
			if(CMP_DATA & BIT1)
			{
				CMP_fail_cnt++;
			}
		}
		if(CMP_fail_cnt > 2)
		{
			Motor.BLDC.u8FlagMotorIsRunning = 0;			// Motor stop
			e1M1_MainState = M1_MainState_Fault;						// MainState Fault
			e1M1_RunSubState = M1_RunState_Calib;						// RunState Calib
			User_sys.BLDC_SensorlessCtr.sys_error_flg |= SHORT_ERROR;				
			#if EN_BRAKE
					BLDC_Brake();
			#else
					BLDC_Stop();			
			#endif
		}
	}	
}

/*******************************************************************************
 �������ƣ�    void SysTick_Handler(void)
 ����������    ϵͳ�δ�ʱ�ж�
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2021/11/15      V1.0        Li        ����
 *******************************************************************************/
void SysTick_Handler(void)
{
	
}

/*******************************************************************************
 �������ƣ�    void MCPWM0_IRQHandler(void)
 ����������    MCPWM0�жϴ�������
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2021/11/15      V1.0        Li        ����
 *******************************************************************************/
void MCPWM0_IRQHandler(void)
{
  if(MCPWM_EIF & FAIL1_IF)
  {	 
		Motor.BLDC.u8FlagMotorIsRunning = 0;			// Motor stop
		e1M1_MainState = M1_MainState_Fault;						// MainState Fault
		e1M1_RunSubState = M1_RunState_Calib;						// RunState Calib
		User_sys.BLDC_SensorlessCtr.sys_error_flg |= SHORT_ERROR;				
		#if EN_BRAKE
				BLDC_Brake();
		#else
				BLDC_Stop();			
		#endif
     MCPWM_EIF = FAIL1_IF;
  }  
}


/*******************************************************************************
 �������ƣ�    void DMA_IRQHandler(void)
 ����������    DMA�жϴ�������
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2021/11/15      V1.0        Li        ����
 *******************************************************************************/
void DMA_IRQHandler(void)
{
  DMA_IF = 0x0f;
}



/*******************************************************************************
 �������ƣ�    void MCPWM1_IRQHandler(void)
 ����������    MCPWM1�жϴ�������
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2021/11/15      V1.0        Li        ����
 *******************************************************************************/
void MCPWM1_IRQHandler(void)
{
	
}


/*******************************************************************************
 �������ƣ�    void SW_IRQHandler(void)
 ����������    �����жϴ�������
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2021/11/15      V1.0        Li        ����
 *******************************************************************************/
void SW_IRQHandler(void)
{

}

/*******************************************************************************
 �������ƣ�    void UART_IRQHandler(void)
 ����������    UART�жϴ�������
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2021/11/15      V1.0        Li        ����
 *******************************************************************************/
void UART_IRQHandler(void)
{
	#if (UART0_FUNCTION == ENABLE_FUNCTION)
 	if (UART_IF & UART_IF_SendOver) //��������ж�
	{
		UART_IF = UART_IF_SendOver;
		UART_Flag = 0;
	}
	if (UART_IF & UART_IF_RcvOver) //��������ж�
	{
		UART_IF = UART_IF_RcvOver;
  // Rx, move data from UART FIFO to buffer
    rxd_comm0.buffer[rxd_comm0.write_pt] = UART0->BUFF;
    rxd_comm0.write_pt = (rxd_comm0.write_pt + 1) % UART0_BUF_SIZE;
    rxd_comm0.cnt++;	
	} 
	#endif	
}

/*******************************************************************************
 �������ƣ�    void WAKE_IRQHandler(void)
 ����������    ���߻����жϴ�������
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2021/11/15      V1.0        Li        ����
 *******************************************************************************/
void WAKE_IRQHandler(void)
{
    while(1);
}

/*******************************************************************************
 �������ƣ�    void GPIO_IRQHandler(void)
 ����������    GPIO�жϴ�������
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2021/11/15      V1.0        Li        ����
 *******************************************************************************/
void GPIO_IRQHandler(void)
{
	if(EXTI_IF & GPIO_P05_EXTI_IF) /* �ж��Ƿ�ΪP0.5�ⲿ�ж�*/
	{
    EXTI_IF = GPIO_P05_EXTI_IF;  /* ���P0.5�ⲿ�жϱ�־λ*/		
	}
}

/*******************************************************************************
 �������ƣ�    void I2C_IRQHandler(void)
 ����������    I2C�жϴ�������
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2021/11/15      V1.0        Li        ����
 *******************************************************************************/
void I2C_IRQHandler(void)
{

}

/*******************************************************************************
 �������ƣ�    void SPI_IRQHandler(void)
 ����������    SPI�жϴ�������
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2021/11/15      V1.0        Li        ����
 *******************************************************************************/
void SPI_IRQHandler(void)
{

}

/************************ (C) COPYRIGHT SNANER SEMICONDUCTOR **********************/
/* ------------------------------END OF FILE------------------------------------ */



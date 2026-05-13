/*******************************************************************************
 * ��Ȩ���� (C)2015, SNANER SEMICONDUCTOR Co.ltd
 *
 * �ļ����ƣ� Main.c
 * �ļ���ʶ��
 * ����ժҪ�� ����������
 * ����˵���� ��
 * ��ǰ�汾�� V 1.0
 * ��    �ߣ� Li
 * ������ڣ� 2020��8��5��
 *
 * �޸ļ�¼1��
 * �޸����ڣ� 2020��8��16��
 * �� �� �ţ� V 1.0
 * �� �� �ˣ� Li
 * �޸����ݣ� ����
 *
 *******************************************************************************/
#include "main.h"
#include "PID.h"
#include "MC_Drive.h"
#include "SpeedRamp.h"
#include "motor_structure.h"
#include "M1_StateMachine.h"
#include "UR_Ctrl.h"
#include "control_mode.h"   /* Phase 7.1 */
#include "motor_mode.h"     /* Phase 12  */
#if MODULE_FLASH_CONFIG
#include "flash_config.h"
#endif

#if MODULE_PROTOCOL_BIPROPELLANT_BIN
extern void setup_uart_protocol(void);
extern void protocol_drain_rx(void);
extern void protocol_drive_tick(void);
#endif
/* Simple-protocol exports come from UR_Ctrl.h, already included transitively */


/*******************************************************************************
 �������ƣ�    int main(void)
 ����������    ���������
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2020/8/5      V1.0             Li          ����
 *******************************************************************************/

int main(void)
{
  /* FIX2-011: verify .data was copied correctly by Reset_Handler */
  { extern void startup_canary_check(void); startup_canary_check(); }

  SoftDelay(20000);      /* ��ʱ�ȴ�Ӳ����ʼ���ȶ� 20000--2.68ms */  //
  SoftDelay(20000);      /* ��ʱ�ȴ�Ӳ����ʼ���ȶ� 20000--2.68ms */  //

	Hardware_init();			 /* Ӳ����ʼ�� */

	/* Phase 12 — load flash override (if any), then detect mode.
	 * Override values: 0=auto, 1=force Hall, 2=force Sensorless. */
	#if MODULE_FLASH_CONFIG
	flash_config_load();
	#endif
	#if MODULE_MOTOR_AUTODETECT && MODULE_FLASH_CONFIG
	if      (g_flash_cfg.motor_mode_override == 1) g_motor_mode = MOTOR_MODE_HALL;
	else if (g_flash_cfg.motor_mode_override == 2) g_motor_mode = MOTOR_MODE_SENSORLESS;
	else                                            motor_mode_detect();
	#else
	motor_mode_detect();
	#endif

	Init_Parameter();

	InitPI();
	sys_init();            /* ϵͳ��ʼ�� */

	#if MODULE_PROTOCOL_BIPROPELLANT_BIN
		setup_uart_protocol();   /* bipropellant init + callbacks + param registry */
	#endif

	MosTest();
	LED_ON
	FG_ON
	
	////��λ�����޵�ֵ�����޸�ֵ��������ֵ���������ֵ
	InitNormalization(VSP_START_VALUE,VSP_MAX_VALUE,MIN_PWM_DUTY,MAX_PWM_DUTY,&RP);		// �ٶȿ���
	InitNormalization(VSP_START_VALUE,VSP_MAX_VALUE,MOTOR_SPEED_MIN_RPM,MOTOR_SPEED_MAX_RPM,&SPEED_Cmd);		// �ٶȱջ�

	#if EN_HALL_PHASE_COMPENSATION		
		InitNormalization(START_SPEED_COMPENSATION,MAX_SPEED_COMPENSATION,MIN_ADVANCE_ANGLE,MAX_ADVANCE_ANGLE,&Speed_PHASE_COMPENSATION_Cmd);		// ��λ����
	#endif
	
	// read CWCCW
	CWCCW_PowerOn();	
	
	// ����ǰ��ȡHall�ź�
	hall_info = HALL_GetFilterValue();    	/* ȡ��HALLֵ���˲����*/
	Motor.BLDC.u8HallValueRenew = hall_info;
	// ��������ת, ��ȡ��һ��hall�ź�
	if(Motor.BLDC.u8Direction == CCW)
	{
		Motor.BLDC.u8HallValueRenew = (0x07) & (~Motor.BLDC.u8HallValueRenew);
	}
	Motor.BLDC.u8HallValueOld = Motor.BLDC.u8HallValueRenew;
	
	for (;;)
	{
			Task_Scheduler();
	}
}


/*******************************************************************************
 �������ƣ�    void Task_Scheduler(void)
 ����������    ��ʱ��Ƭ������Ⱥ���
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2022/5/30      V1.0                     ����
 *******************************************************************************/
void Task_Scheduler(void)
{
    if(User_sys.struTaskScheduler.bTimeCnt1ms >= TASK_SCHEDU_1MS)
    {   
			/* 1�����¼���������� */
			User_sys.struTaskScheduler.bTimeCnt1ms = 0;
			
			// Speed command Input — Phase 7.1: dispatch through control_mode (default MODE_VSP keeps original behavior)
			control_mode_dispatch();
			
			// ������λ����
			Phase_COMPENSATION_Cal();
			
			// ��ѹ�������
			Vol_protect();											
			
			// ��ת���
			Motor_Block_Protect();
			
			// ��������
			Current_Limit();
							
			/* �ּ��������� */
			OVER_current_protect(); 
			
			// MOS �¶ȱ���		
			MOS_TEMP_protect();

			/*  Statemachine */
			sM1_STATE[e1M1_MainState]();
    }  

    if( User_sys.struTaskScheduler.nTimeCnt10ms >= TASK_SCHEDU_10MS )
    {   /* 10�����¼���������� */
        User_sys.struTaskScheduler.nTimeCnt10ms = 0;
			
				/* ���LEDָʾ */
				Motor_LED_DISP();
			
				/* ���ת���趨 */
				CWCCW_ReadIO();

				/* FIX-019: protocol RX drain moved into 10 ms slot.
				 * Each iteration handles bytes received since last call (256-byte
				 * ring buffer can hold >250 ms of 9600 baud traffic; safe margin). */
				#if MODULE_PROTOCOL_BIPROPELLANT_BIN
				protocol_drain_rx();
				protocol_drive_tick();
				#elif MODULE_PROTOCOL_SIMPLE
				UartDealRX();
				UartDealTX();
				#endif
		}

    if( User_sys.struTaskScheduler.nTimeCnt100ms >= TASK_SCHEDU_100MS )
		{
			User_sys.struTaskScheduler.nTimeCnt100ms = 0;
			#if MODULE_FLASH_CONFIG
			{ extern void flash_config_tick(void);  flash_config_tick(); }
			#endif
		}
}

/************************ (C) COPYRIGHT SNANER SEMICONDUCTOR **********************/
/* ------------------------------END OF FILE------------------------------------ */




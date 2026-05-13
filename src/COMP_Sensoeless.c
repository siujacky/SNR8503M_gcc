#include "MC_Parameter.h"

#if MODULE_MOTOR_SENSORLESS

/* FIX2-004: opt into table renaming. Must be before sensorless_adapter.h include. */
#define SENSORLESS_ADAPTER_RENAME_TABLES

#include "basic.h"
#include "global_variable.h"
#include "motor_structure.h"
#include "sensorless_adapter.h"   /* Phase 13: Motor_1M1st, CMP_ZeroCross_lib, BLDC_Communication alias */
#include "COMP_Sensorless.h"
#include "MC_Drive.h"
#include "M1_StateMachine.h"
#include "SpeedRamp.h"

uint8_t CMP_PhaseTab[6] = {5,4,6,2,3,1};
uint8_t Motor_step_tab_cw[6] = {1,2,3,4,5,0};
uint8_t Motor_step_tab_ccw[6] = {5,0,1,2,3,4};

// �Ƚ�����������
uint8_t phase_rise_tab_cw[6] = {0,1,0,1,0,1};
uint8_t phase_rise_tab_ccw[6] = {1,0,1,0,1,0};

void CMP_SNS_Motor_AutoCommutation(void);
void CMP_SNS_Motor_Align(void);
void Motor_Freerun_Detect(void);
void INIT_FreerunDetect(void);
/**
  * @brief  COMP sensorless mode, Motor Align/drag function
  * @retval None
  */
void CMP_SNS_Motor_Align(void)
{
		Motor_1M1st.BLDC_CMP.u32RampTime++;
		
		// Motor drag step cnt
		if(Motor_1M1st.BLDC_CMP.u8ramp_cnt == 0)
		{
			Motor_1M1st.BLDC_CMP.u8ramp_cnt++;
			
			Motor_1M1st.BLDC.u8FlagMotorIsRunning = 1;						// Motor is running flag
			
			/** Initialize first step of Motor startup   */
			// Motor step 
			User_sys.BLDC_SensorlessCtr.communicate_Step = 0;
			
			// Hall state
			User_sys.BLDC_SensorlessCtr.Hallstate = CMP_PhaseTab[User_sys.BLDC_SensorlessCtr.communicate_Step];	
			
			// Motor Communication
			BLDC_Communication(User_sys.BLDC_SensorlessCtr.Hallstate);
			
			MCPWM_UPDATE = 0;
			MCPWM_TH00 = 	- MOTOR_STARTUP_PWMDUTY;
			MCPWM_TH01 = 		MOTOR_STARTUP_PWMDUTY;
			MCPWM_TH10 = 	-	MOTOR_STARTUP_PWMDUTY;
			MCPWM_TH11 = 		MOTOR_STARTUP_PWMDUTY;
			MCPWM_TH20 = 	-	MOTOR_STARTUP_PWMDUTY;
			MCPWM_TH21 = 		MOTOR_STARTUP_PWMDUTY;
			MCPWM_UPDATE = 0xff;
			PWMOutputs(MCPWM0,ENABLE);

			// Detect edge & Motor next step
			if(Motor_1M1st.BLDC.u8Direction == CW)
			{
				CMP_ZeroCross_lib.u8CMP_Zero_phase_rise = phase_rise_tab_cw[User_sys.BLDC_SensorlessCtr.communicate_Step];				// Sensorless detect edge							
				User_sys.BLDC_SensorlessCtr.communicate_Step = Motor_step_tab_cw[User_sys.BLDC_SensorlessCtr.communicate_Step];		// Next step
			}
			else
			{
				CMP_ZeroCross_lib.u8CMP_Zero_phase_rise = phase_rise_tab_ccw[User_sys.BLDC_SensorlessCtr.communicate_Step];				// Sensorless detect edge	
				User_sys.BLDC_SensorlessCtr.communicate_Step = Motor_step_tab_ccw[User_sys.BLDC_SensorlessCtr.communicate_Step];	// Next step
			}

			Motor_1M1st.BLDC_CMP.u8ControlStep = MOTOR_RUN;				// Motor control status
			CMP_ZeroCross_lib.u32CMP_Zero_speed_cnt = MAX_SPEED_CNT; 		// Motor Minimum speed
			CMP_ZeroCross_lib.u32CMP_Zero_phase_max = MAX_SPEED_CNT;
			CMP_ZeroCross_lib.u32CMP_Zero_phase_min = MIN_SPEED_CNT;
			
			Motor_1M1st.BLDC_CMP.u8FlagFreewheelAvoiding = 0;
			Motor_1M1st.BLDC_CMP.u32FreewheelAvoidcnt = 0;
			Motor_1M1st.BLDC_CMP.u32FreewheelAvoidDey = 0;

			Motor_1M1st.BLDC_CMP.u32ElectricalStepcnt = 0;				// Init speed
			Motor_1M1st.BLDC_CMP.u32ElectricalStepValue = MAX_SPEED_CNT;			// Init speed
			Motor_1M1st.BLDC_CMP.u32ElectricalPeriod = (6*MAX_SPEED_CNT);			// Init speed
			Motor_1M1st.BLDC_CMP.u32ElectricalPeriodcnt = 0;	// Init speed
		}
}

/**
  * @brief  COMP sensorless mode, Motor Commutation function
  * @retval None
  */
void CMP_SNS_Motor_AutoCommutation(void)
{
		static  uint8_t j;
		{
				Motor_1M1st.BLDC_CMP.u32ElectricalStepcnt++;		// Step cnt
				Motor_1M1st.BLDC_CMP.u32ElectricalPeriodcnt++;	// Period cnt
			
				if(Motor_1M1st.BLDC_CMP.u8FlagFreewheelAvoiding == 1)		// Avoid motor Free-wheel
				{
					Motor_1M1st.BLDC_CMP.u32FreewheelAvoidcnt++;					// time cnt
					if(Motor_1M1st.BLDC_CMP.u32FreewheelAvoidcnt >= Motor_1M1st.BLDC_CMP.u32FreewheelAvoidDey)		// Finished motor Free-wheel
					{					
						Motor_1M1st.BLDC_CMP.u32FreewheelAvoidcnt = 0;
						Motor_1M1st.BLDC_CMP.u8FlagFreewheelAvoiding = 0;
						
						Motor_1M1st.BLDC_CMP.u32phase_cnt = Motor_1M1st.BLDC_CMP.u32FreewheelAvoidDey;							// update motor phase time cnt
						CMP_ZeroCross_lib.u32CMP_Zero_speed_cnt += Motor_1M1st.BLDC_CMP.u32FreewheelAvoidDey;							// update motor speed time cnt
						
						#if EN_PHASE_COMP
						if (Motor_1M1st.BLDC_CMP.u32closeloop_count > 200)																					//delay
						{
							Motor_1M1st.BLDC_CMP.u32phase_cnt += (u32)(Motor_1M1st.BLDC_CMP.u32FreewheelAvoidDey * PHASE_COMP_LEAD_ANGLE);
						}
						#endif
					}
				}
				else		// BEMF zero-cross detect Function
				{		
					Motor_1M1st.BLDC_CMP.u32phase_cnt++;		// phase time cnt
					
					/** Get COMP Output value	*/
					Motor_1M1st.BLDC_CMP.u32COMP_OUT_temp = CMP0_OUT_LEVEL();			// Read-out COMP Value temp
					CMP_ZeroCross_lib.u8CMP_Zero_OUT_Value = Motor_1M1st.BLDC_CMP.u32COMP_OUT_temp  >> 8;
					
					/** BEMF zero-cross detect sub*/
					COMP_Motor_Zero_Detect(&CMP_ZeroCross_lib);		// Sensorless Zero-crossing detect & delay 30�� Electrical angle

					if(Motor_1M1st.BLDC_CMP.u32phase_cnt >= CMP_ZeroCross_lib.u32CMP_Zero_speed_cnt)		// Motor communication
					{					
						// Hall state
						User_sys.BLDC_SensorlessCtr.Hallstate = CMP_PhaseTab[User_sys.BLDC_SensorlessCtr.communicate_Step];	
						
						// Motor Communication
						BLDC_Communication(User_sys.BLDC_SensorlessCtr.Hallstate);
			
						if(Motor_1M1st.BLDC.u8Direction == CW)
						{
							CMP_ZeroCross_lib.u8CMP_Zero_phase_rise = phase_rise_tab_cw[User_sys.BLDC_SensorlessCtr.communicate_Step];		// Rising/falling edge
							User_sys.BLDC_SensorlessCtr.communicate_Step = Motor_step_tab_cw[User_sys.BLDC_SensorlessCtr.communicate_Step];		// Next step
						}
						else
						{
							CMP_ZeroCross_lib.u8CMP_Zero_phase_rise = phase_rise_tab_ccw[User_sys.BLDC_SensorlessCtr.communicate_Step];	// Rising/falling edge
							User_sys.BLDC_SensorlessCtr.communicate_Step = Motor_step_tab_ccw[User_sys.BLDC_SensorlessCtr.communicate_Step];		// Next step
						}
	
						{		// ��¼����ת������
							Motor_1M1st.BLDC_CMP.u32ElectricalStepValue = Motor_1M1st.BLDC_CMP.u32ElectricalStepcnt;		// Motor sigle step time cnt
							Motor_1M1st.BLDC_CMP.u32ElectricalStepcnt = 0;
							
							if(j > 5)
							{
								j = 0;
							}
							
							// ����ת�ٱջ�����	
							Motor_1M1st.BLDC_CMP.u32MotorElePhaseValue[j] = Motor_1M1st.BLDC_CMP.u32ElectricalStepValue;
							
							// ���ڶ�ת���
							Motor_1M1st.BLDC_CMP.u32MotorElePhaseCnt[j] = Motor_1M1st.BLDC_CMP.u32ElectricalStepValue;
							
							j++;
						}
										
						if(User_sys.BLDC_SensorlessCtr.Hallstate == 1)
						{
							// һ�������������һ��PWM��
							FG_ONOFF;
							
							Motor_1M1st.BLDC_CMP.u32ElectricalPeriod = Motor_1M1st.BLDC_CMP.u32ElectricalPeriodcnt;		// Motor Electrical Period cnt
							Motor_1M1st.BLDC_CMP.u32ElectricalPeriodcnt = 0;
							
							Motor_1M1st.BLDC_CMP.u8FlagGetMotorElePeriod = 1;
							
							if(Motor_1M1st.BLDC_CMP.u32closeloop_count < 2000)
							{
									Motor_1M1st.BLDC_CMP.u32closeloop_count++;
							}
						}
						else if(User_sys.BLDC_SensorlessCtr.Hallstate == 6)
						{
							// һ�������������һ��PWM��
							FG_ONOFF;	
						}
						
						// avoid Motor Free-wheel time
						Motor_1M1st.BLDC_CMP.u32FreewheelAvoidDey = Motor_1M1st.BLDC_CMP.u32phase_cnt;								
						Motor_1M1st.BLDC_CMP.u32FreewheelAvoidDey >>= 2;			// delay 15�� Electrical angle
						Motor_1M1st.BLDC_CMP.u8FlagFreewheelAvoiding = 1;			// Avoid Motor Free-wheel flag
						Motor_1M1st.BLDC_CMP.u32FreewheelAvoidcnt = 0;

						Motor_1M1st.BLDC_CMP.u32phase_cnt = 0;			// Important			
					}
				}
		}			
}

/**
  * @brief  COMP sensorless mode, Motor Freerun Detect function
  * @retval None
  */
void Motor_Freerun_Detect(void)
{
	if(Motor_1M1st.BLDC_CMP.u8FlagDetectMotorFreerunnfinish == 0)			// ������˳�����
	{
		Motor_1M1st.BLDC_CMP.u32DetectFreerunTimecnt++;	// ˳����, ����ʱ�����, ��ȡת������

		/** Get COMP Output value	*/
		Motor_1M1st.BLDC_CMP.u32COMP_OUT_temp = CMP0_OUT_LEVEL();			// Read-out COMP Value temp
		CMP_ZeroCross_lib.u8CMP_Zero_OUT_Value = Motor_1M1st.BLDC_CMP.u32COMP_OUT_temp  >> 8;	// COMP Value
		
		// ���й������
		Freerun_Zero_Detect(&CMP_ZeroCross_lib);		// Lib
		
		if(CMP_ZeroCross_lib.u8CMP_FreerunstepNew != Motor_1M1st.BLDC_CMP.u8FreerunstepOld)						// ��⵽�����
		{
				Motor_1M1st.BLDC_CMP.u32speed_cnt_temp = Motor_1M1st.BLDC_CMP.u32DetectFreerunTimecnt;		// ��¼ת������
				CMP_ZeroCross_lib.u32CMP_Zero_speed_cnt = Motor_1M1st.BLDC_CMP.u32speed_cnt_temp;
				Motor_1M1st.BLDC_CMP.u32phase_cnt = 0;
//				Motor_1M1st.BLDC_CMP.u32speed_cnt_temp >>= 1;				// ��������ʱ������15�㿪ͨPWM
			
				Motor_1M1st.BLDC_CMP.u8FreerunstepOld = CMP_ZeroCross_lib.u8CMP_FreerunstepNew;						// ���±Ƚ������״̬			

				Motor_1M1st.BLDC_CMP.u8Freerunstepchangecnt++;			// ����������
				if(Motor_1M1st.BLDC_CMP.u8Freerunstepchangecnt >= 5)
				{					
					// ˳����OK
					Motor_1M1st.BLDC_CMP.u8FlagDetectMotorFreerunnfinish = 1;
					Motor_1M1st.BLDC_CMP.u8FlagDetectMotorFreerunTailwind = 1;
					Motor_1M1st.BLDC_CMP.u8FlagDetectMotorFreerunAgainstwind = 0;
					Motor_1M1st.BLDC_CMP.u8Freerunstepchangecnt = 0;
				}
				
				// ��������				
				Motor_1M1st.BLDC_CMP.u32DetectFreerunTimecnt = 0;				// ת����������, ������һ��ת������
				CMP_ZeroCross_lib.u32CMP_Freerunstep_1_cnt = 0;
				CMP_ZeroCross_lib.u32CMP_Freerunstep_0_cnt = 0;
				
				if(Motor_1M1st.BLDC_CMP.u8FlagDetectMotorFreerunnfinish == 0)
				{	
						// ������һ���������
						// Hall state
						User_sys.BLDC_SensorlessCtr.Hallstate = CMP_PhaseTab[User_sys.BLDC_SensorlessCtr.communicate_Step];	
						
						// Change COMP Input signal
						BLDC_COMP_Input_Only(User_sys.BLDC_SensorlessCtr.Hallstate);
			
						if(Motor_1M1st.BLDC.u8Direction == CW)
						{
							CMP_ZeroCross_lib.u8CMP_Zero_phase_rise = phase_rise_tab_cw[User_sys.BLDC_SensorlessCtr.communicate_Step];		// Rising/falling edge
							User_sys.BLDC_SensorlessCtr.communicate_Step = Motor_step_tab_cw[User_sys.BLDC_SensorlessCtr.communicate_Step];		// Next step
						}
						else
						{
							CMP_ZeroCross_lib.u8CMP_Zero_phase_rise = phase_rise_tab_ccw[User_sys.BLDC_SensorlessCtr.communicate_Step];	// Rising/falling edge
							User_sys.BLDC_SensorlessCtr.communicate_Step = Motor_step_tab_ccw[User_sys.BLDC_SensorlessCtr.communicate_Step];		// Next step
						}
				}
		}
	}
	
	// ˳/��������
	if(Motor_1M1st.BLDC_CMP.u8FlagDetectMotorFreerunnfinish == 1)		
	{
		if(Motor_1M1st.BLDC_CMP.u8FlagDetectMotorFreerunTailwind == 1)		// ˳��״̬
		{
//				Motor_1M1st.BLDC_CMP.u32phase_cnt = Motor_1M1st.BLDC_CMP.u32speed_cnt_temp;		// �Ƿ���ʱ��PWM	
				Motor_1M1st.BLDC_CMP.u32speed_cnt_temp--;
				Motor_1M1st.BLDC_CMP.u32phase_cnt++;
			
				if( Motor_1M1st.BLDC_CMP.u32phase_cnt >= Motor_1M1st.BLDC_CMP.u32speed_cnt_temp )	// ��⵽��������ʱ��PWM
				{	
//					Motor_1M1st.BLDC_CMP.u32phase_cnt = 0;
					
					Motor_1M1st.BLDC_CMP.u8FlagDetectMotorFreerunTailwind = 0;
					Motor_1M1st.BLDC_CMP.u8FlagDetectMotorFreerunnfinish = 0;
					Motor_1M1st.BLDC_CMP.u8FlagDetectMotorFreerunning = 0;	
					
					// ����״̬��
					e1M1_RunSubState = M1_RunState_Spin;								/* Run Sub State */
					Motor_1M1st.BLDC_CMP.u8ControlStep = MOTOR_RUN;
					
					Motor_1M1st.BLDC.u8FlagMotorIsRunning = 1;						// Motor is running flag
					
					Motor_1M1st.BLDC_CMP.u32MotorRPMFilte_cnt = STARTUP_DRAG_TIME;
					
					RP.OutEn = 0;
					RP.State = 0;
					RP.Out = 0;
					RPValue.Dest = 0;
					RPValue.Act = 0;
					
					SPEED_Cmd.OutEn = 0;
					SPEED_Cmd.State = 0;
					SPEED_Cmd.Out = 0;
					SPEEDValue.Dest = 0;
					SPEEDValue.Act = 0;					
					
		
					// ��ͨ����
					// Hall state
					User_sys.BLDC_SensorlessCtr.Hallstate = CMP_PhaseTab[User_sys.BLDC_SensorlessCtr.communicate_Step];	

					// Motor Communication
					BLDC_Communication(User_sys.BLDC_SensorlessCtr.Hallstate);
					
					// ��ʼ��PWM���	
					MCPWM_UPDATE = 0;
					MCPWM_TH00 = 	- MIN_PWM_DUTY;
					MCPWM_TH01 = 		MIN_PWM_DUTY;
					MCPWM_TH10 = 	-	MIN_PWM_DUTY;
					MCPWM_TH11 = 		MIN_PWM_DUTY;
					MCPWM_TH20 = 	-	MIN_PWM_DUTY;
					MCPWM_TH21 = 		MIN_PWM_DUTY;		
					MCPWM_UPDATE = 0xff;
					PWMOutputs(MCPWM0,ENABLE);
					
					if(Motor_1M1st.BLDC.u8Direction == CW)
					{
						CMP_ZeroCross_lib.u8CMP_Zero_phase_rise = phase_rise_tab_cw[User_sys.BLDC_SensorlessCtr.communicate_Step];		// Rising/falling edge
						User_sys.BLDC_SensorlessCtr.communicate_Step = Motor_step_tab_cw[User_sys.BLDC_SensorlessCtr.communicate_Step];		// Next step
					}
					else
					{
						CMP_ZeroCross_lib.u8CMP_Zero_phase_rise = phase_rise_tab_ccw[User_sys.BLDC_SensorlessCtr.communicate_Step];	// Rising/falling edge
						User_sys.BLDC_SensorlessCtr.communicate_Step = Motor_step_tab_ccw[User_sys.BLDC_SensorlessCtr.communicate_Step];		// Next step
					}

					Motor_1M1st.BLDC_CMP.u8FlagDetectMotorFreerunTailwind = 0;
					Motor_1M1st.BLDC_CMP.u8FlagDetectMotorFreerunAgainstwind = 0;

					Motor_1M1st.BLDC_CMP.u32RampTime = 0;

//					Motor_1M1st.BLDC_CMP.u32ElectricalPeriodcnt = 0;
//					Motor_1M1st.BLDC_CMP.u32FreewheelAvoidcnt = 0;
//					Motor_1M1st.BLDC_CMP.u32FreewheelAvoidDey = 0;
					
						// avoid Motor Free-wheel time
						Motor_1M1st.BLDC_CMP.u32FreewheelAvoidDey = Motor_1M1st.BLDC_CMP.u32phase_cnt;								
						Motor_1M1st.BLDC_CMP.u32FreewheelAvoidDey >>= 2;			// delay 15�� Electrical angle
						Motor_1M1st.BLDC_CMP.u8FlagFreewheelAvoiding = 1;			// Avoid Motor Free-wheel flag
						Motor_1M1st.BLDC_CMP.u32FreewheelAvoidcnt = 0;

						Motor_1M1st.BLDC_CMP.u32phase_cnt = 0;			// Important	
				}
		}
	}
	
	// ˳�����ʱ�䵽
	if(Motor_1M1st.BLDC_CMP.u32DetectFreerunTimecnt > MOTOR_FREERUN_DETECT_CNT)
	{
		Motor_1M1st.BLDC_CMP.u8FlagDetectMotorFreerunnfinish = 0;
		Motor_1M1st.BLDC_CMP.u8FlagDetectMotorFreerunning = 0;
	
		Motor_1M1st.BLDC_CMP.u32DetectFreerunTimecnt = 0;
		Motor_1M1st.BLDC_CMP.u8Freerunstepchangecnt = 0;
		CMP_ZeroCross_lib.u32CMP_Freerunstep_1_cnt = 0;
		CMP_ZeroCross_lib.u32CMP_Freerunstep_0_cnt = 0;
		
		e1M1_RunSubState = M1_RunState_Align;						// ��ת����	
	}		
}

void INIT_FreerunDetect(void)
{		
	Motor_1M1st.BLDC_CMP.u8FlagDetectMotorFreerunning = 1;		
	
	// ����˳���������
	CMP_ZeroCross_lib.u32CMP_Freerunstep_0_Mincnt = MIN_SPEED_CNT;		//�������ٵļ���ֵ

	// Motor step 
	User_sys.BLDC_SensorlessCtr.communicate_Step = 0;
	
	// Hall state
	User_sys.BLDC_SensorlessCtr.Hallstate = CMP_PhaseTab[User_sys.BLDC_SensorlessCtr.communicate_Step];	
	
	BLDC_COMP_Input_Only( User_sys.BLDC_SensorlessCtr.Hallstate );					//���±Ƚ��������ź�, �ȴ��Ƚ����ж�
	
	// Detect edge & Motor next step
	if(Motor_1M1st.BLDC.u8Direction == CW)
	{
		CMP_ZeroCross_lib.u8CMP_FreerunstepNew = 1;	
		Motor_1M1st.BLDC_CMP.u8FreerunstepOld = CMP_ZeroCross_lib.u8CMP_FreerunstepNew;	
		
		User_sys.BLDC_SensorlessCtr.communicate_Step = Motor_step_tab_cw[User_sys.BLDC_SensorlessCtr.communicate_Step];		// Next step
	}
	else
	{
		CMP_ZeroCross_lib.u8CMP_FreerunstepNew = 0;
		Motor_1M1st.BLDC_CMP.u8FreerunstepOld = CMP_ZeroCross_lib.u8CMP_FreerunstepNew;	
		
		User_sys.BLDC_SensorlessCtr.communicate_Step = Motor_step_tab_ccw[User_sys.BLDC_SensorlessCtr.communicate_Step];	// Next step
	}

	Motor_1M1st.BLDC_CMP.u8FlagDetectMotorFreerunnfinish = 0;
	Motor_1M1st.BLDC_CMP.u8FlagDetectMotorFreerunTailwind = 0;
	Motor_1M1st.BLDC_CMP.u8FlagDetectMotorFreerunAgainstwind = 0;

	Motor_1M1st.BLDC_CMP.u32DetectFreerunTimecnt = 0;
	Motor_1M1st.BLDC_CMP.u8Freerunstepchangecnt = 0;
	CMP_ZeroCross_lib.u32CMP_Freerunstep_1_cnt = 0;
	CMP_ZeroCross_lib.u32CMP_Freerunstep_0_cnt = 0;
}


//*****************************************************************//

/* ========================================================================
 * BEMF zero-crossing helpers — implementations of the two sensorless
 * primitives. Originally provided by the closed SNR_BLDC_SNLS_V1p0.lib;
 * these are simple textbook implementations that match the call-site
 * contract observed in the rest of this file and interrupt.c.
 *
 * COMP_Motor_Zero_Detect:
 *   When the comparator output matches the expected edge polarity, the
 *   speed counter decrements (motor running faster than estimated);
 *   when it doesn't match, the counter increments (slower). Clamped to
 *   [phase_min, phase_max]. Canonical closed-loop sensorless speed track
 *   documented in TI SLVA372, ST AN1944, and similar BLDC app notes.
 *
 * Freerun_Zero_Detect:
 *   Integrator-style debounce: each comparator state advances its own
 *   counter and decays the opposite. When a counter passes the threshold
 *   (u32CMP_Freerunstep_0_Mincnt), the current OUT value is committed
 *   as u8CMP_FreerunstepNew. Standard debounce-then-commit pattern for
 *   noisy edge detection on the coasting motor.
 * ======================================================================== */
void COMP_Motor_Zero_Detect(CMP_Zero_TypeDef *p)
{
    if (p == 0) return;

    uint8_t out  = p->u8CMP_Zero_OUT_Value;
    uint8_t rise = p->u8CMP_Zero_phase_rise;

    if (out == rise) {
        if (p->u32CMP_Zero_speed_cnt > p->u32CMP_Zero_phase_min) {
            p->u32CMP_Zero_speed_cnt--;
        }
    } else {
        if (p->u32CMP_Zero_speed_cnt < p->u32CMP_Zero_phase_max) {
            p->u32CMP_Zero_speed_cnt++;
        }
    }
}

void Freerun_Zero_Detect(CMP_Zero_TypeDef *p)
{
    if (p == 0) return;

    uint32_t winning_count;

    if (p->u8CMP_Zero_OUT_Value == 1) {
        p->u32CMP_Freerunstep_1_cnt++;
        if (p->u32CMP_Freerunstep_0_cnt > 0) {
            p->u32CMP_Freerunstep_0_cnt--;
        }
        winning_count = p->u32CMP_Freerunstep_1_cnt;
    } else {
        p->u32CMP_Freerunstep_0_cnt++;
        if (p->u32CMP_Freerunstep_1_cnt > 0) {
            p->u32CMP_Freerunstep_1_cnt--;
        }
        winning_count = p->u32CMP_Freerunstep_0_cnt;
    }

    if (winning_count >= p->u32CMP_Freerunstep_0_Mincnt) {
        p->u8CMP_FreerunstepNew = p->u8CMP_Zero_OUT_Value;
    }
}

#endif /* MODULE_MOTOR_SENSORLESS */

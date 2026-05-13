#include "MC_Parameter.h"
/*-------------------- Includes -----------------------*/
#include "MC_Drive.h"
#include "Global_Variable.h"
#include "motor_structure.h"
#include "M1_StateMachine.h"
#include "SpeedRamp.h"
#include "MC_Parameter.h"
#include "SNR_BLDC_HALL_V1p0.h"

/* Hall-mode commutation tables (Phase 13: gated on MODULE_MOTOR_HALL so
 * sensorless-only profiles don't conflict with the [6] tables in
 * src/COMP_Sensoeless.c). */
#if MODULE_MOTOR_HALL
#if (Hall_Degree_60_or_120 == Degree60)
															 //0 1 2 3 4 5 6 7
uint8_t Motor_step_tab_cw[8] 	= {4,0,8,1,6,8,7,3};		// 0��4��6��7��3��1
uint8_t Motor_step_tab_ccw[8] = {1,3,8,7,0,8,4,6};		// 0��1��3��7��6��4

#else
															 //0 1 2 3 4 5 6 7
uint8_t Motor_step_tab_cw[8]  = {0,5,3,1,6,4,2,0};
uint8_t Motor_step_tab_ccw[8] = {0,3,6,2,5,1,4,0};

#endif
#endif /* MODULE_MOTOR_HALL */

/*------------------- Private variables ---------------*/
void BLDC_Sensor_control(unsigned char Hall_value);
void BLDC_Sensor_Judge(unsigned char Hall_value);

/*------------------ Private functions ----------------*/
void BLDC_Sensor_control(unsigned char Hall_value)
{
    MCPWM_PRT = 0x0000DEAD;
		SYS_WR_PROTECT = 0x7a83;

	#if (Hall_Degree_60_or_120 == Degree60)
    switch(Hall_value) 
		{
			case 5: 
					// A��PWM��B������ C ON����������ع����¼�
					// A pwm | B float | C ON
					MCPWM_IO01 = DRIVER_POLARITY | (0x0c04);		// 
					MCPWM_IO23 = DRIVER_POLARITY | 0x1c;	
					FG_ONOFF;
			break;
			
			
			case 7:
					// A�����գ�B��PWM C��ON ������½��ع����¼�
					// A float | B PWM | C ON
					MCPWM_IO01 = DRIVER_POLARITY | (0x040c);		
					MCPWM_IO23 = DRIVER_POLARITY | 0x1c;			
			break;
				
				
			case 6: 
					// A��ON��B��PWM C������ ����������ع����¼�
					// A ON | B PWM | C float
					MCPWM_IO01 = DRIVER_POLARITY | 0x041c;      	// 
					MCPWM_IO23 = DRIVER_POLARITY | (0x0c);
			break;
				
			
			case 2: 
					// A��ON��B������ C��PWM ������½��ع����¼�
					// A ON | B float | C PWM
					MCPWM_IO01 = DRIVER_POLARITY | (0x0c1c);		// 
					MCPWM_IO23 = DRIVER_POLARITY | 0x04;
					FG_ONOFF;
			break;
				
			
			case 0: 
					// A�����գ�B��ON C��PWM  ����������ع����¼�
					// A float | B ON | C PWM
					MCPWM_IO01 = DRIVER_POLARITY | (0x1c0c);		// 
					MCPWM_IO23 = DRIVER_POLARITY | 0x04;	
			break;
				
			
			case 1: 
					// A��PWM��B��ON C������  ������½��ع����¼�
					// A PWM | B on | C float
					MCPWM_IO01 = DRIVER_POLARITY | 0x1c04;			// 
					MCPWM_IO23 = DRIVER_POLARITY | (0x0c);		
			break;
		}
	
	#else
    switch(Hall_value) 
		{
			case 5: 
					// A��PWM��B������ C ON����������ع����¼�
					// A pwm | B float | C ON
					MCPWM_IO01 = DRIVER_POLARITY | (0x0c04);		// 
					MCPWM_IO23 = DRIVER_POLARITY | 0x1c;	
					FG_ONOFF;
			break;
			
			
			case 4:
					// A�����գ�B��PWM C��ON ������½��ع����¼�
					// A float | B PWM | C ON
					MCPWM_IO01 = DRIVER_POLARITY | (0x040c);		
					MCPWM_IO23 = DRIVER_POLARITY | 0x1c;			
			break;
				
				
			case 6: 
					// A��ON��B��PWM C������ ����������ع����¼�
					// A ON | B PWM | C float
					MCPWM_IO01 = DRIVER_POLARITY | 0x041c;      	// 
					MCPWM_IO23 = DRIVER_POLARITY | (0x0c);
			break;
				
			
			case 2: 
					// A��ON��B������ C��PWM ������½��ع����¼�
					// A ON | B float | C PWM
					MCPWM_IO01 = DRIVER_POLARITY | (0x0c1c);		// 
					MCPWM_IO23 = DRIVER_POLARITY | 0x04;
					FG_ONOFF;
			break;
				
			
			case 3: 
					// A�����գ�B��ON C��PWM  ����������ع����¼�
					// A float | B ON | C PWM
					MCPWM_IO01 = DRIVER_POLARITY | (0x1c0c);		// 
					MCPWM_IO23 = DRIVER_POLARITY | 0x04;	
			break;
				
			
			case 1: 
					// A��PWM��B��ON C������  ������½��ع����¼�
					// A PWM | B on | C float
					MCPWM_IO01 = DRIVER_POLARITY | 0x1c04;			// 
					MCPWM_IO23 = DRIVER_POLARITY | (0x0c);		
			break;
		}
		#endif
		
		MCPWM_PRT = 0x0000CAFE;
		SYS_WR_PROTECT = 0;
}

#if MODULE_MOTOR_HALL
void BLDC_Sensor_Judge(unsigned char Hall_value)
{
	if(Hall_value != Motor.BLDC.u8HallValueOld)
	{
			if(Motor.Control.u16Phase_Compensation_cnt < 200)
			{
				if(Motor.Control.u32MotorRAMP_OK_flag == 1)	// ������ɺ�ʼ����
				{
					Motor.Control.u16Phase_Compensation_cnt++;
				}
			}
			
			if(Motor.Control.u8PhaseChange_ON_COMPENSATION_flag == 1)
			{
				Motor.Control.u8PhaseChange_ON_COMPENSATION_flag = 0;
			}
			else
			{
				// ����
				HALL_LIB.u8HALL_NEW_Value 	= Motor.BLDC.u8HallValueRenew;
				Motor.BLDC.u8HALL_Run_Value = HALL_Update(&HALL_LIB);
				BLDC_Sensor_control(Motor.BLDC.u8HALL_Run_Value);
			}
			// ����hall�ź�	
			Motor.BLDC.u8HallValueOld = Motor.BLDC.u8HALL_Run_Value;
	}
}





#endif /* MODULE_MOTOR_HALL */

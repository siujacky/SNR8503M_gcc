#ifndef __MC_DRIVE_H
#define __MC_DRIVE_H

#include "main.h"
#include "SNR_BLDC_HALL_V1p0.h"

extern u8 CWCCW_bIOInput;	
extern u32 hall_cnt;
extern u8  hall_info;

/* Phase 13: incomplete-type declarations so Hall ([8]) and Sensorless ([6])
 * can both define these with size — the linker resolves by symbol name only. */
extern uint8_t Motor_step_tab_cw[];
extern uint8_t Motor_step_tab_ccw[];

extern void BLDC_Sensor_control(unsigned char Hall_value);
extern void BLDC_Sensor_Judge(unsigned char Hall_value);
extern uint8_t HALL_Update(HALL_NEWVALUE_TypeDef *Motor);

extern void BLDC_Stop(void);
extern void BLDC_Brake(void);
extern void Motor_LED_DISP(void);
extern void CWCCW_PowerOn(void);
extern void CWCCW_ReadIO(void);
extern u16 PiCurrentLimitPWMDuty(u16 Duty_In,u8 newstate);

#endif

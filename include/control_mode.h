#ifndef CONTROL_MODE_H
#define CONTROL_MODE_H

#include <stdint.h>

/*
 * Control mode selector for Task_Scheduler 1ms dispatch.
 * Default is MODE_VSP (existing pre-Option-C behavior).
 * Switching modes is initiated by host writes to 0x03/0x05/0x0D.
 */

typedef enum {
    MODE_VSP        = 0,  /* speed from VSP ADC pot (default; current behavior)        */
    MODE_SPEED_CMD  = 1,  /* speed from 0x03 SpeedData write                            */
    MODE_PWM_DIRECT = 2,  /* duty from 0x0D PWMData write — mapped through VSP pipeline */
    MODE_POSITION   = 3,  /* target ticks from 0x05 PositionIncr — closed-loop PI       */
} ctrl_mode_t;

extern volatile ctrl_mode_t g_ctrl_mode;

/* Targets set by motion params handlers (Phase 7.2) */
extern volatile int32_t g_target_pwm_left;     /* for MODE_PWM_DIRECT (signed)         */
extern volatile int32_t g_target_speed_mm_s;   /* for MODE_SPEED_CMD (mm/s)            */

/* Called once per ms from Task_Scheduler. Dispatches to the right control path
 * based on g_ctrl_mode, then runs VSP_Control_Motor() to materialize the
 * setpoint into PWM. */
void control_mode_dispatch(void);

#endif

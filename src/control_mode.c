/*
 * control_mode.c — Phase 7.1 dispatch layer.
 *
 * Inserts between Task_Scheduler 1ms and VSP_Control_Motor. The default mode
 * (MODE_VSP) reproduces the existing pre-Option-C behavior exactly; switching
 * to other modes is initiated by host writes to the motion parameters
 * (0x03, 0x05, 0x0D) registered in protocol_params_motion.c.
 *
 * Design choice vs PLAN.md 7.4: position-control mode does NOT add a new
 * M1_RunSubState. The motor state machine continues to run Calib→...→Spin
 * unchanged; control_mode_dispatch() chooses what feeds u16VSP_filt_value
 * during normal operation. Cleaner separation of concerns — the state machine
 * is responsible for "is the motor energised", the dispatch decides "what
 * setpoint feeds the speed loop".
 */

#include <stdint.h>
#include "basic.h"
#include "MC_Parameter.h"
#include "Global_Variable.h"
#include "motor_structure.h"
#include "M1_StateMachine.h"
#include "control_mode.h"

extern u16 procHander(u16 hallbarADvalue);
extern void VSP_Control_Motor(void);
extern void position_control_tick(void);   /* Phase 7.3 — provided weakly below if missing */

volatile ctrl_mode_t g_ctrl_mode         = MODE_VSP;
volatile int32_t     g_target_pwm_left   = 0;
volatile int32_t     g_target_speed_mm_s = 0;

/* Map a signed PWM target (range ≈ ±speed_max_power) to unsigned u16VSP
 * (range 0..VSP_MAX_VALUE) and update direction accordingly. */
static void apply_pwm_direct(void)
{
    int32_t pwm = g_target_pwm_left;
    if (pwm < 0) {
        Motor.BLDC.u8Direction = CCW;
        pwm = -pwm;
    } else {
        Motor.BLDC.u8Direction = CW;
    }
    /* Clip to VSP range so the existing speed pipeline accepts it. */
    if (pwm > VSP_MAX_VALUE) pwm = VSP_MAX_VALUE;
    Motor.USER.u16VSP_filt_value = (uint16_t)pwm;
}

void control_mode_dispatch(void)
{
    switch (g_ctrl_mode) {

    case MODE_VSP:
    default:
        /* Original pre-Option-C behavior — filter ADC pot, run speed loop. */
        Motor.USER.u16VSP_filt_value = procHander(
            (u16)User_sys.BLDC_SensorlessCtr.nSpeed_ADC);
        VSP_Control_Motor();
        break;

    case MODE_SPEED_CMD:
        /* Host already wrote u16VSP_filt_value through 0x03 handler — skip
         * the ADC pot read but still run the speed-to-RPM conversion. */
        VSP_Control_Motor();
        break;

    case MODE_PWM_DIRECT:
        apply_pwm_direct();
        VSP_Control_Motor();
        break;

    case MODE_POSITION:
        position_control_tick();        /* updates u16VSP_filt_value from Hall error */
        VSP_Control_Motor();
        break;
    }
}

/* Weak fallback so the firmware links even if position_control.c was excluded.
 * Real impl in src/position_control.c overrides this. */
__attribute__((weak)) void position_control_tick(void)
{
    /* Without a position controller, behave as if no input. */
    Motor.USER.u16VSP_filt_value = 0;
}

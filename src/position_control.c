/*
 * position_control.c — Phase 7.3 closed-loop position controller.
 *
 * Reads gains from g_flash_cfg.Position* (Phase 8). If flash isn't loaded
 * yet, the default-initialised values from flash_config_set_defaults()
 * apply — sensible bench values, no surprise behavior.
 *
 * Algorithm: simple PI on Hall-tick error → speed setpoint in u16VSP units.
 * Derivative term ignored unless PositionKdx100 != 0 (rare for positioning).
 *
 * Anti-windup: integral clamped to ±PositionPWMLimit.
 * Output clamped to ±VSP_MAX_VALUE then mapped through direction sign.
 */

#include "MC_Parameter.h"

#if MODULE_CODES_POSCTRL

#include <stdint.h>
#include "basic.h"
#include "motor_structure.h"
#include "hall_accumulator.h"
#include "control_mode.h"
#include "position_control.h"
#include "flash_config.h"

volatile int32_t g_target_ticks = 0;
static int32_t   s_integral     = 0;
static int32_t   s_last_error   = 0;

void position_control_init(void)
{
    s_integral   = 0;
    s_last_error = 0;
    g_target_ticks = g_hall_ticks_abs;
}

void position_control_add_mm(int32_t delta_mm)
{
    /* Convert mm → ticks. (2πR mm per HALL_TICKS_PER_REV ticks) */
    int32_t delta_ticks = (int32_t)((float)delta_mm * (float)HALL_TICKS_PER_REV
                                    / (2.0f * 3.1415926f * (float)WHEEL_RADIUS_MM));
    g_target_ticks += delta_ticks;
    g_ctrl_mode = MODE_POSITION;
}

void position_control_set_target_mm(int32_t abs_mm)
{
    int32_t abs_ticks = (int32_t)((float)abs_mm * (float)HALL_TICKS_PER_REV
                                  / (2.0f * 3.1415926f * (float)WHEEL_RADIUS_MM));
    g_target_ticks = abs_ticks;
    g_ctrl_mode = MODE_POSITION;
}

/* Called from control_mode_dispatch when g_ctrl_mode == MODE_POSITION. */
void position_control_tick(void)
{
    int32_t current = g_hall_ticks_abs;
    int32_t target  = g_target_ticks;
    int32_t error   = target - current;

    int32_t Kp_x100   = g_flash_cfg.PositionKpx100;
    int32_t Ki_x100   = g_flash_cfg.PositionKix100;
    int32_t Kd_x100   = g_flash_cfg.PositionKdx100;
    int32_t out_lim   = g_flash_cfg.PositionPWMLimit;
    if (out_lim <= 0) out_lim = VSP_MAX_VALUE;

    /* Integrate. Pre-clamp to avoid wind-up (Q15-ish: divide by 100 at output). */
    s_integral += error;
    int32_t i_lim = out_lim * 100;
    if (s_integral >  i_lim) s_integral =  i_lim;
    if (s_integral < -i_lim) s_integral = -i_lim;

    /* Derivative (often disabled when Kd_x100 == 0). */
    int32_t deriv = error - s_last_error;
    s_last_error = error;

    /* Output: divide by 100 because gains are ×100. */
    int32_t out = (Kp_x100 * error + Ki_x100 * (s_integral / 100) + Kd_x100 * deriv) / 100;

    /* Clip to ±out_lim */
    if (out >  out_lim) out =  out_lim;
    if (out < -out_lim) out = -out_lim;

    /* Map to direction + u16VSP magnitude */
    if (out < 0) {
        Motor.BLDC.u8Direction = CCW;
        out = -out;
    } else {
        Motor.BLDC.u8Direction = CW;
    }
    if (out > VSP_MAX_VALUE) out = VSP_MAX_VALUE;

    /* FIX-018: dead-band — within ±2 ticks of target, output zero (instead of
     * forcing up to VSP_START which would over-correct and oscillate).
     * FIX2-002: also reset s_last_error so re-entry from deadband doesn't
     * compute a stale derivative kick. */
    int32_t abs_error = (error < 0) ? -error : error;
    if (abs_error <= 2) {
        out = 0;
        s_integral = 0;
        s_last_error = error;
    } else if (out < VSP_START_VALUE) {
        out = VSP_START_VALUE;  /* minimum drive to overcome stiction */
    }

    Motor.USER.u16VSP_filt_value = (uint16_t)out;
}

#endif /* MODULE_CODES_POSCTRL */

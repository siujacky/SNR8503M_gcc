#ifndef POSITION_CONTROL_H
#define POSITION_CONTROL_H

#include <stdint.h>

/*
 * Phase 7.3 — closed-loop position control fed by Hall accumulator.
 *
 * Target is in raw Hall ticks (absolute). 0x05 PositionIncr writes adjust it
 * by a per-call delta. PI controller computes a speed setpoint (mm/s) and
 * pushes it into the motor's speed pipeline by writing u16VSP_filt_value.
 *
 * The Kp / Ki / Kd / output-limit gains live in g_flash_cfg (Phase 8). On
 * startup before flash_config_load runs, the weak default in this module
 * provides sensible bench values so the motor doesn't oscillate violently.
 */

extern volatile int32_t g_target_ticks;        /* absolute target position, in Hall ticks */

void position_control_init(void);              /* call once after Hardware_init */
void position_control_tick(void);              /* 1 ms tick, called from control_mode_dispatch */

/* Convenience for 0x05 handler: shift target by delta-mm. */
void position_control_add_mm(int32_t delta_mm);

/* For 0x06 handler: set absolute target in mm */
void position_control_set_target_mm(int32_t abs_mm);

#endif

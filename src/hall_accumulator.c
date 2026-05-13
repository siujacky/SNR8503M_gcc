/*
 * hall_accumulator.c — counts Hall edges with direction awareness.
 *
 * Phase 5.2 infrastructure. Phase 5 itself doesn't surface this over UART
 * (no parameters reference it), but Phase 6.1 plugs it into 4 parameter
 * handlers (0x02, 0x04, 0x07, 0x0C) so adding it now means Phase 6 is purely
 * additive.
 *
 * Period tracking is intentionally derived from a free-running tick counter
 * captured at the same time as the edge so we don't need an extra hardware
 * timer. We use `g_ms_tick` (from timebase.c) at ms resolution — coarse but
 * adequate for low-speed telemetry. Phase 7 will replace with the TIMER0
 * input-capture path already used by ICP_Loader for higher resolution.
 */

#include "MC_Parameter.h"

#if MODULE_MOTOR_HALL

#include <stdint.h>
#include "hall_accumulator.h"

extern volatile uint32_t g_ms_tick;   /* from timebase.c */

volatile int32_t  g_hall_ticks_abs       = 0;
volatile uint32_t g_hall_ticks_period_us = 0;

static uint32_t s_last_edge_ms = 0;

void hall_accum_tick(uint8_t direction)
{
    if (direction) g_hall_ticks_abs++;
    else           g_hall_ticks_abs--;

    /* FIX-011: g_ms_tick is written by ADC_IRQ (priority 1) and read here in
     * HALL_IRQ (priority 0). HALL preempts ADC, so we may observe g_ms_tick
     * up to 1 ms stale if preemption hits between pwm_div check and tick
     * increment. Period-telemetry is informational only, so the 1 ms slop is
     * acceptable; documented here for future maintainers. */
    uint32_t now = g_ms_tick;
    uint32_t delta_ms = now - s_last_edge_ms;
    s_last_edge_ms = now;

    /* convert ms→us; cap at u32 max for stationary motor */
    g_hall_ticks_period_us = (delta_ms < 4000000U) ? (delta_ms * 1000U) : 0xFFFFFFFFU;
}

#endif /* MODULE_MOTOR_HALL */

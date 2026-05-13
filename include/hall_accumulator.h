#ifndef HALL_ACCUMULATOR_H
#define HALL_ACCUMULATOR_H

#include <stdint.h>

/* Phase 5.2 infrastructure used by Phase 6+ position telemetry.
 * Counts Hall edges; signed so reverse motion decrements. */

extern volatile int32_t  g_hall_ticks_abs;          /* unbounded counter */
extern volatile uint32_t g_hall_ticks_period_us;    /* time between last 2 edges, microseconds */

/* Called from HALL_IRQHandler. `direction`: 1=CW (increment), 0=CCW (decrement). */
void hall_accum_tick(uint8_t direction);

#endif

/*
 * timebase.c — millisecond tick counter for the bipropellant protocol.
 *
 * Driven by ADC_IRQHandler (called once per PWM period). With PWM at 16 kHz,
 * 16 ticks ≈ 1 ms (62.5 µs each; the rounding error is ~3 ppm over hours).
 */

#include <stdint.h>
#include <assert.h>
#include "MC_Parameter.h"
#include "timebase.h"

/* FIX-008: divisor derived from PWM_FREQ. Was hardcoded to 16 (assumed 16 kHz PWM). */
#define PWM_TICKS_PER_MS  (PWM_FREQ / 1000)
_Static_assert((PWM_FREQ % 1000) == 0,
    "timebase.c: PWM_FREQ must be a whole number of kHz for ms tick to be exact");

volatile uint32_t g_ms_tick = 0;
static uint16_t pwm_div = 0;

/* Called from ADC_IRQHandler (one PWM cycle = 1/PWM_FREQ seconds). */
void timebase_pwm_tick(void)
{
    if (++pwm_div >= PWM_TICKS_PER_MS) {
        pwm_div = 0;
        g_ms_tick++;
    }
}

uint32_t get_ms_tick(void)
{
    return g_ms_tick;
}

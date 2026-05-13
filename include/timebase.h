#ifndef TIMEBASE_H
#define TIMEBASE_H

#include <stdint.h>

/* Increment internal ms counter; call from ADC_IRQHandler (1 PWM cycle). */
void timebase_pwm_tick(void);

/* Return milliseconds since boot (approx; based on PWM clock). */
uint32_t get_ms_tick(void);

#endif

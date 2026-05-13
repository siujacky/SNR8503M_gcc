/*
 * startup_canary.c — FIX2-011: regression-test the startup .data copy.
 *
 * If the GNU startup ever regresses to a state that fails to copy .data
 * correctly to RAM (e.g., the FIX-001 bug returning), these non-zero
 * initialized values will arrive at RAM as zero or garbage.
 *
 * The canaries are placed in .data (because they're initialized to non-zero
 * values). Reset_Handler should copy them from Flash to RAM. If any value
 * mismatches the expected pattern at boot time, g_data_copy_ok is set false.
 *
 * Called from main() before any motor logic. A future diagnostic param could
 * surface g_data_copy_ok over bipropellant.
 */

#include <stdint.h>
#include "feature_config.h"

volatile uint32_t __data_canary[4] __attribute__((used)) = {
    0xCAFE0001, 0xCAFE0002, 0xCAFE0003, 0xCAFE0004
};

volatile uint8_t  g_data_copy_ok = 0;   /* set true at startup_canary_check */

void startup_canary_check(void)
{
    g_data_copy_ok = (__data_canary[0] == 0xCAFE0001) &&
                     (__data_canary[1] == 0xCAFE0002) &&
                     (__data_canary[2] == 0xCAFE0003) &&
                     (__data_canary[3] == 0xCAFE0004);
}

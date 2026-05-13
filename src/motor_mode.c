/*
 * motor_mode.c — Phase 12: Hall vs Sensorless detection + global mode.
 *
 * Detection algorithm (only runs when both motor modules compiled):
 *   1. After Hardware_init() configured Hall pins and other peripherals
 *   2. Sample HALL_GetFilterValue() N times spaced 5 ms apart
 *   3. If ANY reading is in 0x01-0x06 (valid Hall combinations) → MODE_HALL
 *   4. Otherwise (all 0x00 or all 0x07 → no sensors wired) → MODE_SENSORLESS
 *
 * Override via flash code 0xB1 (handled in flash_config / protocol_params_flash):
 *   0xB1 == 0 → auto (use detection result)
 *   0xB1 == 1 → force MOTOR_MODE_HALL
 *   0xB1 == 2 → force MOTOR_MODE_SENSORLESS
 *
 * Mode is FIXED at boot. Runtime switching is rejected (would mid-cycle
 * commutation). To change: write 0xB1 + reboot.
 */

#include <stdint.h>
#include "feature_config.h"
#include "motor_mode.h"

/* Default to MOTOR_MODE_HALL when only Hall compiled (it's the only choice).
 * When only Sensorless compiled, default = MOTOR_MODE_SENSORLESS.
 * When both compiled, motor_mode_detect() picks at boot. */
#if MODULE_MOTOR_HALL && !MODULE_MOTOR_SENSORLESS
volatile motor_mode_t g_motor_mode = MOTOR_MODE_HALL;
#elif MODULE_MOTOR_SENSORLESS && !MODULE_MOTOR_HALL
volatile motor_mode_t g_motor_mode = MOTOR_MODE_SENSORLESS;
#else
volatile motor_mode_t g_motor_mode = MOTOR_MODE_HALL;  /* tentative; overwritten by detect() */
#endif

#if MODULE_MOTOR_AUTODETECT

#include "basic.h"
#include "snr8503x_hall.h"
#include "delay.h"
#if MODULE_FLASH_CONFIG
#include "flash_config.h"
#endif

/* SoftDelay calibration: ~7500 ticks ≈ 1 ms at 48 MHz */
#define TICKS_PER_MS         7500U
#define DEFAULT_WINDOW_MS    50U     /* used when MODULE_FLASH_CONFIG is off */
#define SAMPLE_INTERVAL_MS   5U
/* FIX-010: majority voting — require 70% of samples to be valid before
 * committing to MODE_HALL. Resists spurious noise from disconnected lines. */
#define MAJORITY_PERCENT     70U

motor_mode_t motor_mode_detect(void)
{
#if MODULE_FLASH_CONFIG
    /* FIX-009: use Flash-configured window (default 50 ms; 5 ms per sample). */
    uint32_t window_ms = g_flash_cfg.hall_detection_window_ms;
    if (window_ms < SAMPLE_INTERVAL_MS) window_ms = SAMPLE_INTERVAL_MS;
    if (window_ms > 500U) window_ms = 500U;     /* sanity cap */
#else
    uint32_t window_ms = DEFAULT_WINDOW_MS;
#endif
    uint32_t sample_count = window_ms / SAMPLE_INTERVAL_MS;
    if (sample_count == 0) sample_count = 1;
    uint32_t threshold = (sample_count * MAJORITY_PERCENT + 99U) / 100U;

    uint32_t valid_count = 0;
    for (uint32_t i = 0; i < sample_count; i++) {
        uint8_t h = (uint8_t)HALL_GetFilterValue();
        if (h >= 0x01 && h <= 0x06) {
            valid_count++;
            /* FIX2-005: early-exit once threshold is reached. Absence of valid
             * samples still waits the full window — we can't shortcut a "no" */
            if (valid_count >= threshold) {
                g_motor_mode = MOTOR_MODE_HALL;
                return g_motor_mode;
            }
        }
        SoftDelay(TICKS_PER_MS * SAMPLE_INTERVAL_MS);
    }

    g_motor_mode = (valid_count >= threshold) ? MOTOR_MODE_HALL : MOTOR_MODE_SENSORLESS;
    return g_motor_mode;
}

#else  /* !MODULE_MOTOR_AUTODETECT — single-mode build */

motor_mode_t motor_mode_detect(void)
{
    /* No detection necessary; mode is hardcoded at compile time. */
    return g_motor_mode;
}

#endif

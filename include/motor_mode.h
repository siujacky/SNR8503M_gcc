#ifndef MOTOR_MODE_H
#define MOTOR_MODE_H

#include <stdint.h>
#include "feature_config.h"

/*
 * Phase 12 — Motor mode selection (Hall vs Sensorless).
 *
 * If only one motor module is compiled (MODULE_MOTOR_AUTODETECT == 0),
 * g_motor_mode is hardcoded at compile time and detection is a no-op.
 *
 * When both modules are compiled (MODULE_MOTOR_AUTODETECT == 1),
 * motor_mode_detect() reads Hall pins after Hardware_init and picks
 * the appropriate mode at boot. Override via flash code 0xB1.
 */

typedef enum {
    MOTOR_MODE_HALL       = 0,
    MOTOR_MODE_SENSORLESS = 1,
} motor_mode_t;

extern volatile motor_mode_t g_motor_mode;

/* Sample Hall pins for the configured window and return MOTOR_MODE_HALL
 * if at least one valid (0x01-0x06) reading was observed; otherwise
 * MOTOR_MODE_SENSORLESS. Called at boot after Hardware_init.
 *
 * If MODULE_MOTOR_AUTODETECT==0, returns the compile-time-only mode and
 * does not actually sample hardware. */
motor_mode_t motor_mode_detect(void);

#endif

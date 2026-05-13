#ifndef FLASH_CONFIG_H
#define FLASH_CONFIG_H

#include <stdint.h>
#include "protocol.h"   /* for PROTOCOL_ADC_SETTINGS */

/*
 * Phase 8.2 — Flash-backed configuration block.
 *
 * Stored in the last 4 KB sector of Flash (0x0003F000 - 0x0003FFFF, see
 * linker.ld __flash_config_start). On boot, flash_config_load() validates
 * the magic word; if mismatch (blank flash, corrupted, or older version),
 * defaults are written automatically.
 *
 * Layout matches hoverboard's FlashContent so 0x80-0xA0 host writes line up
 * with the corresponding fields here. Code 0x80 magic is THIS struct's
 * magic field; reads/writes through bipropellant update the RAM mirror
 * (g_flash_cfg), and a periodic dirty-check in Task_Scheduler commits.
 */

#define FLASH_CONFIG_MAGIC    0xDEADBEEF
/* FIX2-001: bumped to 2 after adding the trailing CRC field. v1 saved configs
 * (from pre-FIX-013 builds) fail validation and revert to defaults — intended.
 * Document in release notes: upgrading firmware invalidates persisted tunings.
 *
 * Note on adjacent codes: 0xB0/B1/B2 are NOT FlashContent fields — they're
 * registered separately and live in g_motor_mode + motor_mode_override +
 * hall_detection_window_ms. The HoverboardEnable code 0xA0 IS in this struct. */
#define FLASH_CONFIG_VERSION  2

typedef struct {
    uint32_t magic;                       /* 0x80 — validates the block            */
    uint32_t version;                     /* future schema migration               */
    int32_t  PositionKpx100;              /* 0x81 — Position PID Kp ×100           */
    int32_t  PositionKix100;              /* 0x82 — Position PID Ki ×100           */
    int32_t  PositionKdx100;              /* 0x83 — Position PID Kd ×100           */
    int32_t  PositionPWMLimit;            /* 0x84 — Position output ceiling (VSP units) */
    int32_t  SpeedKpx100;                 /* 0x85 — Speed PID Kp ×100  (advisory)  */
    int32_t  SpeedKix100;                 /* 0x86 — Speed PID Ki ×100  (advisory)  */
    int32_t  SpeedKdx100;                 /* 0x87 — Speed PID Kd ×100  (advisory)  */
    int32_t  SpeedPWMIncrementLimit;      /* 0x88 — speed ramp limit (advisory)    */
    int32_t  MaxCurrLim;                  /* 0x89 — current limit in 0.01 A units  */
    PROTOCOL_ADC_SETTINGS adc;            /* 0x90 — ADC settings (mostly no-op here) */
    uint8_t  HoverboardEnable;            /* 0xA0 — gate flag                      */
    uint8_t  motor_mode_override;         /* 0xB1 — 0=auto, 1=hall, 2=sensorless   */
    uint16_t hall_detection_window_ms;    /* 0xB2 — auto-detect sample window      */
    uint32_t crc;                         /* FIX-013: CRC32 over all preceding fields */
} __attribute__((aligned(4))) FlashContent_t;
/* FIX-005: explicit 4-byte alignment so sizeof is a multiple of 4 word size used
 * by Read_More_Flash. CRC field (FIX-013) extends the struct cleanly. */

/* RAM mirror — single source of truth at runtime.
 * On boot, populated by flash_config_load() from Flash.
 * Modified by bipropellant param handlers; committed by flash_config_save(). */
extern FlashContent_t g_flash_cfg;

/* Initialise mirror with hardcoded defaults — used when Flash is blank/invalid */
void flash_config_set_defaults(void);

/* Read from Flash; validates magic. Falls back to defaults if invalid.
 * Must be called once after Hardware_init. */
void flash_config_load(void);

/* Commit RAM mirror to Flash. Throttled: at most one call per ~60s
 * (count via g_ms_tick) and only if g_flash_cfg has been marked dirty.
 * Safe to call frequently — the throttle/dirty checks are inside. */
void flash_config_request_save(void);

/* Called periodically (e.g. 100ms slot) to actually do the save when due. */
void flash_config_tick(void);

/* FIX-012: number of consecutive Flash-program failures since last success.
 * Cleared on successful save. */
uint8_t flash_config_save_failed_count(void);

#endif

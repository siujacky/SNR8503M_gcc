/*
 * protocol_params_flash.c — Phase 8.3 — Flash-backed parameter registry.
 *
 * Wires codes 0x80-0xA0 onto fields of g_flash_cfg. Writes go directly into
 * the RAM mirror and request a future save (debounced 60 s, deferred while
 * motor is running). Reads return the live mirror values.
 *
 * NOTE on Phase 8.4 status:
 *   - Position PID (0x81-0x84) IS live — position_control_tick() reads
 *     g_flash_cfg.Position* directly.
 *   - PositionPWMLimit (0x84) IS live — same.
 *   - Speed PID (0x85-0x87), SpeedPWMIncrementLimit (0x88), MaxCurrLim (0x89)
 *     are mirror-only. Writes update the mirror and persist to Flash, but
 *     the existing speed loop / current-limit checks continue to use the
 *     compile-time #define values. Full 8.4 refactor will replace these.
 *   - 0x90 ADC settings, 0xA0 HoverboardEnable — mirror-only stubs.
 *
 * Code 0x80 magic write triggers an immediate save (a "commit now" hook).
 */

#include "MC_Parameter.h"

#if MODULE_FLASH_CONFIG

#include <stdint.h>
#include <stddef.h>
#include "basic.h"
#include "protocol.h"
#include "flash_config.h"
#if MODULE_MOTOR_AUTODETECT
#include "motor_mode.h"
#endif

/* Wrapper handler that triggers save request on writes to any flash-backed code. */
static void fn_flash_param(PROTOCOL_STAT *s, PARAMSTAT *param,
                           unsigned char cmd, PROTOCOL_MSG3full *msg)
{
    fn_defaultProcessing(s, param, cmd, msg);
    if (cmd == PROTOCOL_CMD_WRITEVAL) {
        flash_config_request_save();
    }
}

static void register_one(PROTOCOL_STAT *s, unsigned char code, const char *desc,
                         void *ptr, int len)
{
    /* FIX-002: pool was [12] but 15 register_one() calls happen when
     * MODULE_MOTOR_AUTODETECT=1 (12 standard + 3 motor-mode). Bumped to 16. */
    static PARAMSTAT pool[16];
    static int pool_used = 0;
    if (pool_used >= (int)(sizeof(pool)/sizeof(pool[0]))) return;
    PARAMSTAT *p = &pool[pool_used++];
    p->code        = code;
    p->description = (char*)desc;
    p->uistr       = NULL;
    p->ui_type     = UI_NONE;
    p->ptr         = ptr;
    p->len         = len;
    p->fn          = fn_flash_param;
    setParam(s, p);
}

void register_flash_params(PROTOCOL_STAT *s)
{
    register_one(s, 0x80, "FlashContent.magic",
                 &g_flash_cfg.magic, sizeof(g_flash_cfg.magic));
    register_one(s, 0x81, "PositionKpx100",
                 &g_flash_cfg.PositionKpx100, sizeof(g_flash_cfg.PositionKpx100));
    register_one(s, 0x82, "PositionKix100",
                 &g_flash_cfg.PositionKix100, sizeof(g_flash_cfg.PositionKix100));
    register_one(s, 0x83, "PositionKdx100",
                 &g_flash_cfg.PositionKdx100, sizeof(g_flash_cfg.PositionKdx100));
    register_one(s, 0x84, "PositionPWMLimit",
                 &g_flash_cfg.PositionPWMLimit, sizeof(g_flash_cfg.PositionPWMLimit));
    register_one(s, 0x85, "SpeedKpx100",
                 &g_flash_cfg.SpeedKpx100, sizeof(g_flash_cfg.SpeedKpx100));
    register_one(s, 0x86, "SpeedKix100",
                 &g_flash_cfg.SpeedKix100, sizeof(g_flash_cfg.SpeedKix100));
    register_one(s, 0x87, "SpeedKdx100",
                 &g_flash_cfg.SpeedKdx100, sizeof(g_flash_cfg.SpeedKdx100));
    register_one(s, 0x88, "SpeedPWMIncrementLimit",
                 &g_flash_cfg.SpeedPWMIncrementLimit,
                 sizeof(g_flash_cfg.SpeedPWMIncrementLimit));
    register_one(s, 0x89, "MaxCurrLim",
                 &g_flash_cfg.MaxCurrLim, sizeof(g_flash_cfg.MaxCurrLim));
    register_one(s, 0x90, "ADCSettings",
                 &g_flash_cfg.adc, sizeof(g_flash_cfg.adc));
    register_one(s, 0xA0, "HoverboardEnable",
                 &g_flash_cfg.HoverboardEnable, sizeof(g_flash_cfg.HoverboardEnable));
#if MODULE_MOTOR_AUTODETECT
    /* FIX-007: 0xB0 motor_mode_active is read-only.  Host should write 0xB1
     * (override) and reboot to change mode — runtime mid-cycle mode switch
     * is unsafe. We use a separate PARAMSTAT entry with the read-only handler. */
    extern volatile motor_mode_t g_motor_mode;
    {
        static PARAMSTAT ro_active;
        ro_active.code        = 0xB0;
        ro_active.description = "motor_mode_active";
        ro_active.uistr       = NULL;
        ro_active.ui_type     = UI_CHAR;
        ro_active.ptr         = (void*)&g_motor_mode;
        ro_active.len         = sizeof(g_motor_mode);
        ro_active.fn          = fn_defaultProcessingReadOnly;
        setParam(s, &ro_active);
    }
    register_one(s, 0xB1, "motor_mode_override",
                 &g_flash_cfg.motor_mode_override, sizeof(g_flash_cfg.motor_mode_override));
    register_one(s, 0xB2, "hall_detection_window_ms",
                 &g_flash_cfg.hall_detection_window_ms, sizeof(g_flash_cfg.hall_detection_window_ms));
#endif

    /* FIX-022 + FIX2-009 + FIX2-006: diagnostic counters (read-only).
     * Host reads these to surface silent failures. */
    {
        extern uint8_t flash_config_save_failed_count(void);
        static volatile uint8_t s_save_failed_mirror;
        s_save_failed_mirror = flash_config_save_failed_count();
        static PARAMSTAT ro_save_fail;
        ro_save_fail.code        = 0xB5;
        ro_save_fail.description = "flash_save_failed_count";
        ro_save_fail.uistr       = NULL;
        ro_save_fail.ui_type     = UI_CHAR;
        ro_save_fail.ptr         = (void*)&s_save_failed_mirror;
        ro_save_fail.len         = sizeof(s_save_failed_mirror);
        ro_save_fail.fn          = fn_defaultProcessingReadOnly;
        setParam(s, &ro_save_fail);
    }
#if MODULE_MOTOR_SENSORLESS
    {
        extern volatile uint32_t g_sensorless_bad_phase_count;
        static PARAMSTAT ro_bad_phase;
        ro_bad_phase.code        = 0xB6;
        ro_bad_phase.description = "sensorless_bad_phase_count";
        ro_bad_phase.uistr       = NULL;
        ro_bad_phase.ui_type     = UI_LONG;
        ro_bad_phase.ptr         = (void*)&g_sensorless_bad_phase_count;
        ro_bad_phase.len         = sizeof(g_sensorless_bad_phase_count);
        ro_bad_phase.fn          = fn_defaultProcessingReadOnly;
        setParam(s, &ro_bad_phase);
    }
#endif
    /* FIX-022: features bitfield 0xB4 — reports build-time feature flags.
     * Host can sanity-check what was compiled in without inspecting profiles. */
    {
        static uint32_t s_features =
              (MODULE_MOTOR_HALL                 ? (1U <<  0) : 0)
            | (MODULE_MOTOR_SENSORLESS           ? (1U <<  1) : 0)
            | (MODULE_PROTOCOL_SIMPLE            ? (1U <<  2) : 0)
            | (MODULE_PROTOCOL_BIPROPELLANT_BIN  ? (1U <<  3) : 0)
            | (MODULE_PROTOCOL_BIPROPELLANT_ASCII? (1U <<  4) : 0)
            | (MODULE_CODES_STANDARD             ? (1U <<  5) : 0)
            | (MODULE_CODES_SNR                  ? (1U <<  6) : 0)
            | (MODULE_CODES_POSITION             ? (1U <<  7) : 0)
            | (MODULE_CODES_MOTION               ? (1U <<  8) : 0)
            | (MODULE_CODES_POSCTRL              ? (1U <<  9) : 0)
            | (MODULE_FLASH_CONFIG               ? (1U << 10) : 0)
            | (MODULE_MOTOR_AUTODETECT           ? (1U << 11) : 0);
        static PARAMSTAT ro_features;
        ro_features.code        = 0xB4;
        ro_features.description = "firmware_features";
        ro_features.uistr       = NULL;
        ro_features.ui_type     = UI_LONG;
        ro_features.ptr         = (void*)&s_features;
        ro_features.len         = sizeof(s_features);
        ro_features.fn          = fn_defaultProcessingReadOnly;
        setParam(s, &ro_features);
    }
}

#endif /* MODULE_FLASH_CONFIG */

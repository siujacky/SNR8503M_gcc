/*
 * flash_config.c — Phase 8.2 + 8.3 helpers.
 *
 * The RAM mirror g_flash_cfg is THE source of truth at runtime. All
 * bipropellant param handlers read/write it directly. The throttled save
 * mechanism only commits to Flash; it does NOT change live behavior.
 *
 * Save policy (cheap to call, expensive to actually run):
 *   - Mark dirty on every change
 *   - Periodic tick (100ms) checks: is dirty AND >= 60 s since last save?
 *     If yes, erase + program the 4 KB sector and clear dirty
 *   - Defensive guard: skip save while motor is running (g_FlagEnMotorRun=1)
 *     to keep the 1 ms scheduler responsive while Flash is busy
 */

#include "MC_Parameter.h"

#if MODULE_FLASH_CONFIG

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "basic.h"
#include "snr8503x.h"
#include "snr8503x_Flash.h"
#include "motor_structure.h"
#include "flash_config.h"

extern volatile uint32_t g_ms_tick;     /* from timebase.c */

/* Linker symbols — see linker.ld */
extern uint32_t __flash_config_start;
#define FLASH_CONFIG_ADDR ((uint32_t)&__flash_config_start)
#define FLASH_CONFIG_SIZE 0x1000U       /* 4 KB sector */

FlashContent_t g_flash_cfg;

/* FIX2-003: compile-time verification that the struct is 4-byte aligned.
 * Read_More_Flash reads sizeof/4 words; if size isn't a multiple of 4 the
 * last bytes are silently lost. */
_Static_assert((sizeof(FlashContent_t) & 3) == 0,
    "FlashContent_t must be 4-byte aligned for Read_More_Flash");

/* FIX-024: volatile is kept on s_dirty (and s_last_save_ms) even though both
 * are accessed only from main-loop context. Rationale: the request-save and
 * tick functions live in different scheduler slots; without volatile the
 * compiler is free to cache the value across function calls when LTO is
 * enabled in a future build configuration. Cheap insurance against a subtle
 * future bug. */
static volatile uint8_t  s_dirty       = 0;
static volatile uint32_t s_last_save_ms = 0;

/* Save policy knobs */
#define SAVE_DEBOUNCE_MS  60000U   /* coalesce 60 s of changes into 1 erase+program */

void flash_config_set_defaults(void)
{
    memset(&g_flash_cfg, 0, sizeof(g_flash_cfg));

    g_flash_cfg.magic   = FLASH_CONFIG_MAGIC;
    g_flash_cfg.version = FLASH_CONFIG_VERSION;

    /* Position PID — gentle bench defaults; user tunes via 0x81-0x84 */
    g_flash_cfg.PositionKpx100   = 100;     /* Kp = 1.00  */
    g_flash_cfg.PositionKix100   = 0;
    g_flash_cfg.PositionKdx100   = 0;
    g_flash_cfg.PositionPWMLimit = VSP_MAX_VALUE / 2;   /* half VSP swing */

    /* Speed PID — values mirror the compile-time SSum_Kp/Ki/Kc (advisory; the
     * existing speed loop continues to use the #define values until Phase 8.4
     * full refactor lands. Hosts that write these can read back the new value
     * but the speed PID behavior won't change until the refactor.) */
    g_flash_cfg.SpeedKpx100             = 5;    /* Q15(0.05) ≈ 0.05 → ×100 = 5  */
    g_flash_cfg.SpeedKix100             = 0;    /* Q15(0.003) → ≈ 0 at ×100      */
    g_flash_cfg.SpeedKdx100             = 0;
    g_flash_cfg.SpeedPWMIncrementLimit  = VSP_DUTY_ACC_LOAD;

    g_flash_cfg.MaxCurrLim          = MAX_BUS_CURRENT_SETTINT * 100;  /* 18 A → 1800 */
    g_flash_cfg.HoverboardEnable    = 0;
    g_flash_cfg.motor_mode_override = 0;     /* 0 = auto */
    g_flash_cfg.hall_detection_window_ms = 50;
}

/* FIX-013 + FIX2-007: CRC32 (reflected, polynomial 0xEDB88320) using a 16-entry
 * nibble lookup table. ~30 cycles per byte vs ~64 cycles for the bit-by-bit
 * variant; 64 bytes of Flash for the table. About 2x faster than naive impl,
 * 10x slower than a full 256-entry table but with 16x less memory.
 *
 * Reference: classic "half-byte" CRC32 algorithm. */
static const uint32_t fc_crc32_tab[16] = {
    0x00000000U, 0x1DB71064U, 0x3B6E20C8U, 0x26D930ACU,
    0x76DC4190U, 0x6B6B51F4U, 0x4DB26158U, 0x5005713CU,
    0xEDB88320U, 0xF00F9344U, 0xD6D6A3E8U, 0xCB61B38CU,
    0x9B64C2B0U, 0x86D3D2D4U, 0xA00AE278U, 0xBDBDF21CU,
};

static uint32_t fc_crc32(const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFU;
    while (len--) {
        crc ^= *p++;
        crc = (crc >> 4) ^ fc_crc32_tab[crc & 0x0FU];
        crc = (crc >> 4) ^ fc_crc32_tab[crc & 0x0FU];
    }
    return ~crc;
}

static uint32_t compute_cfg_crc(const FlashContent_t *cfg)
{
    /* CRC over all fields except `crc` itself (which is last). */
    return fc_crc32(cfg, offsetof(FlashContent_t, crc));
}

void flash_config_load(void)
{
    FlashContent_t tmp;
    Read_More_Flash(FLASH_CONFIG_ADDR, sizeof(tmp) / 4, (uint32_t*)&tmp);

    /* FIX-013: full validation — magic, version, AND CRC. */
    if (tmp.magic == FLASH_CONFIG_MAGIC &&
        tmp.version == FLASH_CONFIG_VERSION &&
        tmp.crc == compute_cfg_crc(&tmp)) {
        memcpy(&g_flash_cfg, &tmp, sizeof(tmp));
    } else {
        /* Blank flash or corrupted — restore defaults. */
        flash_config_set_defaults();
        s_dirty = 1;
    }

    /* FIX-004: initialise the save-throttle clock so first save is debounced
     * like every other save. Without this, the first dirty-save would fire
     * within the first 100 ms scheduler slot regardless of debounce. */
    s_last_save_ms = g_ms_tick;
}

/* Save attempt counter for FIX-012 — exposed for future diagnostic param. */
static volatile uint8_t s_save_failed_count = 0;

static int do_save_now(void)
{
    int ok;
    /* Disable interrupts while erasing + programming. Flash bus is busy
     * for a few ms; ADC/Hall interrupts during this window can mis-time
     * commutation, so we expect the caller to have stopped the motor.
     *
     * FIX-015: also re-check motor-running flag INSIDE the critical section
     * to close the TOCTOU window between the outer guard and erase. */
    __disable_irq();
    if (Motor.BLDC.u8FlagEnMotorRun != 0) {
        __enable_irq();
        return 0;
    }

    extern volatile u32 erase_flag;
    extern volatile u32 progm_flag;

    /* FIX-013: compute CRC just before write so saved copy is self-validating. */
    g_flash_cfg.crc = compute_cfg_crc(&g_flash_cfg);

    erase_flag = 0x9A0D361F;
    EraseSector(FLASH_CONFIG_ADDR);

    progm_flag = 0x9AFDA40C;
    /* FIX-012: capture ProgramPage return; non-zero = success. */
    ok = ProgramPage(FLASH_CONFIG_ADDR, sizeof(g_flash_cfg), (uint8_t*)&g_flash_cfg);

    if (ok) {
        s_dirty = 0;
        s_save_failed_count = 0;
    } else {
        if (s_save_failed_count < 255) s_save_failed_count++;
        /* Leave dirty=1 so the next tick retries. */
    }
    s_last_save_ms = g_ms_tick;

    __enable_irq();
    return ok;
}

void flash_config_request_save(void)
{
    s_dirty = 1;
}

uint8_t flash_config_save_failed_count(void)
{
    return s_save_failed_count;
}

/* Called from Task_Scheduler 100 ms slot. */
void flash_config_tick(void)
{
    if (!s_dirty) return;
    if (Motor.BLDC.u8FlagEnMotorRun != 0) return;   /* never save while spinning */
    uint32_t now = g_ms_tick;
    /* FIX-004: removed `s_last_save_ms != 0` short-circuit; s_last_save_ms is
     * initialised in flash_config_load() so the comparison is always meaningful. */
    if ((now - s_last_save_ms) < SAVE_DEBOUNCE_MS) return;

    do_save_now();
}

#endif /* MODULE_FLASH_CONFIG */

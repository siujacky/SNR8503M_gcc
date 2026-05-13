/*
 * protocol_params_position.c — Position/Hall telemetry parameters.
 *
 * Phase 6.1 — Option C tier 1. Pure additive, read-only handlers.
 *
 * All 4 codes derive from the Hall-tick accumulator (Phase 5.2). No motor
 * behaviour changes from this file. Hosts that don't read these codes see
 * no difference; hosts that do get position + speed in both Hall counts
 * and engineering units.
 *
 * Codes (see OptionC_PLAN.md "Code allocation reference"):
 *   0x02 HallData       — PROTOCOL_HALL_DATA_STRUCT
 *   0x04 Position       — PROTOCOL_POSN
 *   0x07 RawPosition    — 4x i32 (mirror of PROTOCOL_POSN with raw ticks + offsets)
 *   0x0C xytPosn        — PROTOCOL_INTEGER_XYT_POSN (dead-reckoned)
 */

#include "MC_Parameter.h"

#if MODULE_CODES_POSITION

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "basic.h"
#include "motor_structure.h"
#include "hall_accumulator.h"
#include "protocol.h"

/* ---------- Module-local mirrors (populated per-read in handlers) ---------- */
static PROTOCOL_HALL_DATA_STRUCT  g_hall_data;
static PROTOCOL_POSN              g_position;
static PROTOCOL_INTEGER_XYT_POSN  g_xyt;

/* 0x07 has no published struct in protocol.h, so define one matching hoverboard's
 * RawPosition layout: 4×i32 representing raw left/right ticks + offsets. We use
 * `Left*` slots for our single motor; `Right*` stays at zero. */
typedef struct {
    int32_t LeftRaw;
    int32_t RightRaw;
    int32_t LeftOffset;
    int32_t RightOffset;
} RAW_POSITION_T;
static RAW_POSITION_T g_raw_position;

/* User-settable offset (the value written via 0x07 LeftOffset becomes the new zero) */
static volatile int32_t g_position_offset = 0;

/* ---------- Constants pre-computed once to dodge per-read float math ----- */
/* mm per Hall edge = 2π × R / TICKS_PER_REV */
#define MM_PER_TICK_F  ((2.0f * 3.1415926f * (float)WHEEL_RADIUS_MM) / (float)HALL_TICKS_PER_REV)

/* HallPosnMultiplier: "revolutions per tick" = 1 / TICKS_PER_REV */
#define HALL_POSN_MULT_F  (1.0f / (float)HALL_TICKS_PER_REV)

/* Electrical RPM → mm/s: eRPM × 2πR / (60 × poles) */
#define ERPM_TO_MM_PER_S(eRPM) \
    ((int32_t)((float)(eRPM) * (2.0f * 3.1415926f * (float)WHEEL_RADIUS_MM) \
                / (60.0f * (float)MOTOR_POLES)))

/* ---------- Handlers ---------- */

/* 0x02 HallData — populate from accumulator + motor speed */
static void fn_hall_data(PROTOCOL_STAT *s, PARAMSTAT *param,
                         unsigned char cmd, PROTOCOL_MSG3full *msg)
{
    int32_t ticks = g_hall_ticks_abs;            /* atomic read on M0 (LDR) */
    uint32_t eRPM = Motor.Control.u32MotorEleRPM_Final;

    memset(&g_hall_data, 0, sizeof(g_hall_data));
    g_hall_data.HallPosn            = ticks;
    g_hall_data.HallSpeed           = (int32_t)eRPM;     /* electrical RPM */
    g_hall_data.HallPosnMultiplier  = HALL_POSN_MULT_F;
    g_hall_data.HallPosn_mm         = (int32_t)((float)ticks * MM_PER_TICK_F);
    g_hall_data.HallSpeed_mm_per_s  = ERPM_TO_MM_PER_S(eRPM);
    g_hall_data.HallTimeDiff        = g_hall_ticks_period_us;

    fn_defaultProcessingReadOnly(s, param, cmd, msg);
}

/* 0x04 Position — i32 quad: LeftAbsolute, RightAbsolute, LeftOffset, RightOffset */
static void fn_position(PROTOCOL_STAT *s, PARAMSTAT *param,
                        unsigned char cmd, PROTOCOL_MSG3full *msg)
{
    int32_t ticks = g_hall_ticks_abs;

    memset(&g_position, 0, sizeof(g_position));
    g_position.LeftAbsolute = ticks - g_position_offset;
    g_position.LeftOffset   = g_position_offset;

    fn_defaultProcessingReadOnly(s, param, cmd, msg);
}

/* 0x07 RawPosition — same layout as 0x04 but reports the un-offset count */
static void fn_raw_position(PROTOCOL_STAT *s, PARAMSTAT *param,
                            unsigned char cmd, PROTOCOL_MSG3full *msg)
{
    if (cmd == PROTOCOL_CMD_WRITEVAL) {
        /* Host can write to set a new "zero" offset.  */
        fn_defaultProcessing(s, param, cmd, msg);          /* copy bytes into mirror */
        g_position_offset = g_raw_position.LeftOffset;     /* take the LeftOffset field */
        return;
    }

    memset(&g_raw_position, 0, sizeof(g_raw_position));
    g_raw_position.LeftRaw    = g_hall_ticks_abs;
    g_raw_position.LeftOffset = g_position_offset;
    fn_defaultProcessingReadOnly(s, param, cmd, msg);
}

/* 0x0C xytPosn — dead-reckoned X (forward distance), Y=0, theta=0 for single-motor */
static void fn_xyt(PROTOCOL_STAT *s, PARAMSTAT *param,
                   unsigned char cmd, PROTOCOL_MSG3full *msg)
{
    int32_t ticks = g_hall_ticks_abs;

    memset(&g_xyt, 0, sizeof(g_xyt));
    g_xyt.x       = (int32_t)((float)ticks * MM_PER_TICK_F);
    /* y, degrees = 0 (single-track / no steering on this hardware) */

    fn_defaultProcessingReadOnly(s, param, cmd, msg);
}

/* ---------- Registration ---------- */

void register_position_params(PROTOCOL_STAT *s)
{
    {
        static PARAMSTAT p;
        p.code        = 0x02;
        p.description = "HallData";
        p.uistr       = NULL;
        p.ui_type     = UI_NONE;
        p.ptr         = &g_hall_data;
        p.len         = sizeof(g_hall_data);
        p.fn          = fn_hall_data;
        setParam(s, &p);
    }
    {
        static PARAMSTAT p;
        p.code        = 0x04;
        p.description = "Position";
        p.uistr       = NULL;
        p.ui_type     = UI_NONE;
        p.ptr         = &g_position;
        p.len         = sizeof(g_position);
        p.fn          = fn_position;
        setParam(s, &p);
    }
    {
        static PARAMSTAT p;
        p.code        = 0x07;
        p.description = "RawPosition";
        p.uistr       = NULL;
        p.ui_type     = UI_NONE;
        p.ptr         = &g_raw_position;
        p.len         = sizeof(g_raw_position);
        p.fn          = fn_raw_position;
        setParam(s, &p);
    }
    {
        static PARAMSTAT p;
        p.code        = 0x0C;
        p.description = "xytPosn";
        p.uistr       = NULL;
        p.ui_type     = UI_NONE;
        p.ptr         = &g_xyt;
        p.len         = sizeof(g_xyt);
        p.fn          = fn_xyt;
        setParam(s, &p);
    }
}

#endif /* MODULE_CODES_POSITION */

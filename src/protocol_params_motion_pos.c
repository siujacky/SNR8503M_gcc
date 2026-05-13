/*
 * protocol_params_motion_pos.c — Phase 7.3 — Position-control codes.
 *
 *   0x05 PositionIncr  — write delta-mm, engage MODE_POSITION
 *   0x06 PosnData      — set absolute target / read posn-loop telemetry
 *
 * Separated from protocol_params_motion.c because these depend on
 * position_control.c (Phase 7.3) rather than just the dispatch layer.
 */

#include "MC_Parameter.h"

#if MODULE_CODES_POSCTRL

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "basic.h"
#include "motor_structure.h"
#include "hall_accumulator.h"
#include "control_mode.h"
#include "position_control.h"
#include "protocol.h"

static PROTOCOL_POSN_INCR g_posn_incr;
static PROTOCOL_POSN_DATA g_posn_data;

/* mm per Hall tick — precomputed at compile time */
#define MM_PER_TICK_F  ((2.0f * 3.1415926f * (float)WHEEL_RADIUS_MM) / (float)HALL_TICKS_PER_REV)

/* 0x05 PositionIncr — write only (read returns last commanded incr) */
static void fn_posn_incr(PROTOCOL_STAT *s, PARAMSTAT *param,
                         unsigned char cmd, PROTOCOL_MSG3full *msg)
{
    fn_defaultProcessing(s, param, cmd, msg);
    if (cmd == PROTOCOL_CMD_WRITEVAL) {
        position_control_add_mm(g_posn_incr.Left);
        /* Right ignored — single-motor hardware */
    }
}

/* 0x06 PosnData — set absolute target (in mm) and read posn-loop telemetry */
static void fn_posn_data(PROTOCOL_STAT *s, PARAMSTAT *param,
                         unsigned char cmd, PROTOCOL_MSG3full *msg)
{
    if (cmd == PROTOCOL_CMD_READVAL || cmd == PROTOCOL_CMD_SILENTREAD) {
        memset(&g_posn_data, 0, sizeof(g_posn_data));
        g_posn_data.wanted_posn_mm[0]     = (int32_t)((float)g_target_ticks * MM_PER_TICK_F);
        g_posn_data.posn_diff_mm[0]       =
            (int32_t)((float)(g_target_ticks - g_hall_ticks_abs) * MM_PER_TICK_F);
        g_posn_data.posn_speed_demand[0]  = (int32_t)Motor.USER.u16VSP_filt_value;
        fn_defaultProcessingReadOnly(s, param, cmd, msg);
        return;
    }

    fn_defaultProcessing(s, param, cmd, msg);
    if (cmd == PROTOCOL_CMD_WRITEVAL) {
        position_control_set_target_mm(g_posn_data.wanted_posn_mm[0]);
    }
}

void register_motion_pos_params(PROTOCOL_STAT *s)
{
    {
        static PARAMSTAT p;
        p.code        = 0x05;
        p.description = "PositionIncr";
        p.uistr       = NULL;
        p.ui_type     = UI_NONE;
        p.ptr         = &g_posn_incr;
        p.len         = sizeof(g_posn_incr);
        p.fn          = fn_posn_incr;
        setParam(s, &p);
    }
    {
        static PARAMSTAT p;
        p.code        = 0x06;
        p.description = "PosnData";
        p.uistr       = NULL;
        p.ui_type     = UI_NONE;
        p.ptr         = &g_posn_data;
        p.len         = sizeof(g_posn_data);
        p.fn          = fn_posn_data;
        setParam(s, &p);
    }
}

#endif /* MODULE_CODES_POSCTRL */

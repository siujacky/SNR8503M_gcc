/*
 * protocol_params_motion.c — Phase 7.2 — Active control codes.
 *
 *   0x01 sensor_copy   — synth from VSP pot (read-only)
 *   0x03 SpeedData     — set target speed in mm/s, engage MODE_SPEED_CMD
 *   0x0D PWMData       — set direct PWM, engage MODE_PWM_DIRECT
 *   0x0E PWMData.pwm   — subset of 0x0D (just the pwm[2] field)
 *
 * Writes here change motor behaviour. They are NOT engaged by default;
 * the firmware boots in MODE_VSP and stays there until a host explicitly
 * issues a WRITE to 0x03/0x05/0x0D.
 */

#include "MC_Parameter.h"

#if MODULE_CODES_MOTION

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "basic.h"
#include "Global_Variable.h"
#include "motor_structure.h"
#include "control_mode.h"
#include "position_control.h"
#include "protocol.h"

/* ---------- Module-local mirrors ---------- */
static PROTOCOL_SENSOR_FRAME g_sensor;
static PROTOCOL_SPEED_DATA   g_speed_data;
static PROTOCOL_PWM_DATA     g_pwm_data;

/* ---------- Unit conversions ---------- */
/* mm/s → eRPM:  mm/s × 60 × poles / (2π × R) */
static int32_t mms_to_erpm(int32_t mms)
{
    return (int32_t)((float)mms * (60.0f * (float)MOTOR_POLES)
                                 / (2.0f * 3.1415926f * (float)WHEEL_RADIUS_MM));
}

/* eRPM → mm/s (used in telemetry fields) */
static int32_t erpm_to_mms(int32_t eRPM)
{
    return (int32_t)((float)eRPM * (2.0f * 3.1415926f * (float)WHEEL_RADIUS_MM)
                                 / (60.0f * (float)MOTOR_POLES));
}

/* ---------- 0x01 sensor_copy — synth from VSP pot ---------- */
static void fn_sensor(PROTOCOL_STAT *s, PARAMSTAT *param,
                      unsigned char cmd, PROTOCOL_MSG3full *msg)
{
    memset(&g_sensor, 0, sizeof(g_sensor));
    g_sensor.header_00 = 0x00;
    g_sensor.AA_55     = 0xAA;
    /* Map VSP pot 0..1800 → Angle ±1000 around centre. Just a hint to hosts
     * that read 0x01 expecting a hoverboard balance-sensor frame. */
    int32_t vsp_centred = (int32_t)User_sys.BLDC_SensorlessCtr.nSpeed_ADC - 900;
    g_sensor.Angle           = (short)vsp_centred;
    g_sensor.Angle_duplicate = (short)vsp_centred;
    g_sensor.Roll            = 0;

    fn_defaultProcessingReadOnly(s, param, cmd, msg);
}

/* ---------- 0x03 SpeedData ---------- */
static void fn_speed_data(PROTOCOL_STAT *s, PARAMSTAT *param,
                          unsigned char cmd, PROTOCOL_MSG3full *msg)
{
    /* Read leg: populate telemetry fields from current motor state */
    if (cmd == PROTOCOL_CMD_READVAL || cmd == PROTOCOL_CMD_SILENTREAD) {
        memset(&g_speed_data, 0, sizeof(g_speed_data));
        g_speed_data.wanted_speed_mm_per_sec[0] = g_target_speed_mm_s;
        g_speed_data.wanted_speed_mm_per_sec[1] = 0;
        g_speed_data.speed_max_power            = VSP_MAX_VALUE;
        g_speed_data.speed_min_power            = VSP_START_VALUE;
        g_speed_data.speed_minimum_speed        = VSP_OFF_VALUE;
        g_speed_data.speed_diff_mm_per_sec[0]   =
            erpm_to_mms((int32_t)Motor.Control.u32MotorEleRPM_Final)
            - g_target_speed_mm_s;
        g_speed_data.speed_power_demand[0]      = Motor.USER.u16VSP_filt_value;
        fn_defaultProcessingReadOnly(s, param, cmd, msg);
        return;
    }

    /* Write leg: copy bytes into mirror, then apply */
    fn_defaultProcessing(s, param, cmd, msg);

    if (cmd == PROTOCOL_CMD_WRITEVAL) {
        int32_t mms = g_speed_data.wanted_speed_mm_per_sec[0];
        g_target_speed_mm_s = mms;

        /* Negative speed → CCW */
        if (mms < 0) { Motor.BLDC.u8Direction = CCW; mms = -mms; }
        else         { Motor.BLDC.u8Direction = CW;  }

        /* Map mm/s → eRPM → u16VSP (using existing speed pipeline scale).
         * Speed pipeline expects u16VSP_filt_value in the range
         * [VSP_START_VALUE, VSP_MAX_VALUE] which is then normalised to RPM. */
        int32_t eRPM = mms_to_erpm(mms);
        if (eRPM < (int32_t)MOTOR_SPEED_MIN_RPM)
            eRPM = (eRPM == 0) ? 0 : (int32_t)MOTOR_SPEED_MIN_RPM;
        if (eRPM > (int32_t)MOTOR_SPEED_MAX_RPM)
            eRPM = (int32_t)MOTOR_SPEED_MAX_RPM;

        /* Reverse-map RPM into VSP unit. RPM range MIN_RPM..MAX_RPM maps to
         * VSP_START_VALUE..VSP_MAX_VALUE linearly. */
        int32_t span_rpm = MOTOR_SPEED_MAX_RPM - MOTOR_SPEED_MIN_RPM;
        int32_t span_vsp = VSP_MAX_VALUE - VSP_START_VALUE;
        int32_t vsp = (eRPM == 0) ? 0 :
                      VSP_START_VALUE + (eRPM - MOTOR_SPEED_MIN_RPM) * span_vsp / span_rpm;
        if (vsp < 0) vsp = 0;
        Motor.USER.u16VSP_filt_value = (uint16_t)vsp;

        g_ctrl_mode = MODE_SPEED_CMD;
    }
}

/* ---------- 0x0D PWMData ---------- */
static void fn_pwm_data(PROTOCOL_STAT *s, PARAMSTAT *param,
                        unsigned char cmd, PROTOCOL_MSG3full *msg)
{
    fn_defaultProcessing(s, param, cmd, msg);

    if (cmd == PROTOCOL_CMD_WRITEVAL) {
        int32_t pwm = g_pwm_data.pwm[0];
        int32_t mx  = g_pwm_data.speed_max_power;
        if (mx > 0 && pwm >  mx) pwm =  mx;
        if (mx > 0 && pwm < -mx) pwm = -mx;
        g_target_pwm_left = pwm;
        g_ctrl_mode       = MODE_PWM_DIRECT;
    }
}

/* ---------- Registration ---------- */

void register_motion_params(PROTOCOL_STAT *s)
{
    {
        static PARAMSTAT p;
        p.code        = 0x01;
        p.description = "sensor_copy (synth)";
        p.uistr       = NULL;
        p.ui_type     = UI_NONE;
        p.ptr         = &g_sensor;
        p.len         = sizeof(g_sensor);
        p.fn          = fn_sensor;
        setParam(s, &p);
    }
    {
        static PARAMSTAT p;
        p.code        = 0x03;
        p.description = "SpeedData";
        p.uistr       = NULL;
        p.ui_type     = UI_NONE;
        p.ptr         = &g_speed_data;
        p.len         = sizeof(g_speed_data);
        p.fn          = fn_speed_data;
        setParam(s, &p);
    }
    {
        static PARAMSTAT p;
        p.code        = 0x0D;
        p.description = "PWMData";
        p.uistr       = NULL;
        p.ui_type     = UI_NONE;
        p.ptr         = &g_pwm_data;
        p.len         = sizeof(g_pwm_data);
        p.fn          = fn_pwm_data;
        setParam(s, &p);
    }
    /* 0x0E: alias to the pwm[2] sub-field of 0x0D.
     * Hosts that prefer the lighter 8-byte interface can use this. */
    setParamVariable(s, 0x0E, UI_NONE, (void*)g_pwm_data.pwm, sizeof(g_pwm_data.pwm));
}

#endif /* MODULE_CODES_MOTION */

/*
 * protocol_params_standard.c — hoverboard-standard parameter codes.
 *
 * Phase 5.3-5.5: Option B — wire-compat layer.
 *
 * Implements only the standard codes that make sense on single-motor BLDC
 * hardware. Right-motor fields are zeroed; Phase 6+7 fill them with synthesised
 * single-motor data without changing struct shapes here.
 *
 * Code allocations (see OptionC_PLAN.md):
 *   0x08 electrical_measurements  — PROTOCOL_ELECTRICAL_PARAMS, read-only handler
 *   0x09 protocol_enable          — drop-in u8, RW
 *   0x0A disablepoweroff          — accept/ignore stub (no power-off relay)
 *   0x0B debug_out                — accept/ignore stub (we use RTT not UART for debug)
 *   0x21 BuzzerData               — toggles LED once as buzzer surrogate
 */

#include "MC_Parameter.h"

#if MODULE_CODES_STANDARD

#include <stddef.h>
#include <string.h>
#include "basic.h"
#include "hardware_config.h"
#include "Global_Variable.h"
#include "motor_structure.h"
#include "protocol.h"

/* ----- Module-local storage for the dynamically-populated struct ----- */
static PROTOCOL_ELECTRICAL_PARAMS  g_electrical;
static PROTOCOL_BUZZER_DATA        g_buzzer;
static volatile uint8_t            g_disable_poweroff = 0;
static volatile uint8_t            g_debug_out        = 0;

/* ----- Conversion helpers (parameters from MC_Parameter.h) -----
 *
 * FIX-016 note: these use IEEE 754 single-precision floats which are
 * soft-emulated on Cortex-M0 (no FPU). Each multiply/divide is ~50-100 cycles
 * (~1-2 us at 48 MHz). fn_electrical does two per read = ~4 us total. Fine for
 * 100 ms+ subscriptions; avoid subscribing to 0x08 at the 10 ms scheduler rate.
 * For tight real-time use, raw ADC values are available at 0x44 (volt) and
 * 0x45 (current) via SNR custom codes — no float math involved. */
/* batteryVoltage = ADC * 3.6 / 32752 * VOLTAGE_SHUNT_RATIO */
static float adc_to_battery_voltage(int16_t adc)
{
    return (float)adc * (3.6f / 32752.0f) * VOLTAGE_SHUNT_RATIO;
}

/* dcAmps: u32Ibus_Final ADC counts → amps. CURRENT_ADC_PER_A from MC_Parameter.h */
static float adc_to_amps(uint32_t adc_counts)
{
    return (float)adc_counts / (float)CURRENT_ADC_PER_A;
}

/* ----- 0x08 electrical_measurements handler ----- */
/* Read populates struct; write is rejected (electrical_measurements is read-only). */
static void fn_electrical(PROTOCOL_STAT *s, PARAMSTAT *param,
                          unsigned char cmd, PROTOCOL_MSG3full *msg)
{
    /* Populate from current globals on every read. */
    memset(&g_electrical, 0, sizeof(g_electrical));

    g_electrical.bat_raw         = User_sys.BLDC_SensorlessCtr.nBUS_Vol_ADC;
    g_electrical.batteryVoltage  = adc_to_battery_voltage(
        User_sys.BLDC_SensorlessCtr.nBUS_Vol_ADC);
    g_electrical.board_temp_raw  = User_sys.BLDC_SensorlessCtr.nMOS_NTC_ADC;
    g_electrical.dcCurLim        = (int32_t)CURRENT_LIM_VALUE;

    /* motors[0] = our one motor; motors[1] = zeros */
    g_electrical.motors[0].dcAmps      = adc_to_amps(
        User_sys.BLDC_SensorlessCtr.u32Ibus_Final);
    g_electrical.motors[0].dcAmpsAvg   = g_electrical.motors[0].dcAmps;
    g_electrical.motors[0].dcAmpsx100  = (int32_t)(g_electrical.motors[0].dcAmps * 100.0f);
    g_electrical.motors[0].pwm_actual  = (int32_t)Motor.Control.u32MCPWM_TH_temp;
    g_electrical.motors[0].pwm_requested = g_electrical.motors[0].pwm_actual;

    /* Defer the actual default-handler processing now that ptr is populated.  */
    fn_defaultProcessingReadOnly(s, param, cmd, msg);
}

/* ----- 0x21 BuzzerData handler: blink LED on write ----- */
static void fn_buzzer(PROTOCOL_STAT *s, PARAMSTAT *param,
                      unsigned char cmd, PROTOCOL_MSG3full *msg)
{
    fn_defaultProcessing(s, param, cmd, msg);  /* let it copy bytes into g_buzzer */
    if (cmd == PROTOCOL_CMD_WRITEVAL && g_buzzer.buzzerLen > 0) {
        /* No buzzer hardware — flash LED once as a surrogate.  */
        LED_ONOFF;
    }
}

/* ----- Wire all handlers/variables into the param table ----- */
void register_standard_params(PROTOCOL_STAT *s)
{
    /* 0x08 electrical_measurements — full PROTOCOL_ELECTRICAL_PARAMS struct */
    {
        static PARAMSTAT p;
        p.code        = 0x08;
        p.description = "electrical_measurements";
        p.uistr       = NULL;
        p.ui_type     = UI_NONE;
        p.ptr         = &g_electrical;
        p.len         = sizeof(g_electrical);
        p.fn          = fn_electrical;
        setParam(s, &p);
    }

    /* 0x09 protocol_enable — drop-in u8 RW */
    setParamVariable(s, 0x09, UI_CHAR,
                     (void*)&Motor.BLDC.u8FlagEnMotorRun,
                     sizeof(Motor.BLDC.u8FlagEnMotorRun));

    /* 0x0A disablepoweroff — accept/ignore stub */
    setParamVariable(s, 0x0A, UI_CHAR, (void*)&g_disable_poweroff, sizeof(g_disable_poweroff));

    /* 0x0B debug_out — accept/ignore stub (could gate RTT verbosity later) */
    setParamVariable(s, 0x0B, UI_CHAR, (void*)&g_debug_out, sizeof(g_debug_out));

    /* 0x21 BuzzerData — write triggers LED blink */
    {
        static PARAMSTAT p;
        p.code        = 0x21;
        p.description = "BuzzerData (LED surrogate)";
        p.uistr       = NULL;
        p.ui_type     = UI_NONE;
        p.ptr         = &g_buzzer;
        p.len         = sizeof(g_buzzer);
        p.fn          = fn_buzzer;
        setParam(s, &p);
    }
}

#endif /* MODULE_CODES_STANDARD */

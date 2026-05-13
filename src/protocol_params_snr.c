/*
 * protocol_params_snr.c — SNR8503M-specific custom parameters (codes 0x40-0x49).
 *
 * Phase 5.1: moved here from inline registrations in protocol_handlers.c.
 * These codes are SAFE — they're outside the hoverboard-standard range
 * (0x01-0x21, 0x80-0xA0). Use them for SNR-specific diagnostics/control
 * that a generic hoverboard host wouldn't understand anyway.
 *
 * See OptionC_PLAN.md "Code allocation reference" for the complete map.
 */

#include "MC_Parameter.h"

#if MODULE_CODES_SNR

#include <stddef.h>
#include "basic.h"
#include "Global_Variable.h"
#include "motor_structure.h"
#include "M1_StateMachine.h"
#include "protocol.h"

void register_snr_params(PROTOCOL_STAT *s)
{
    /* 0x40 target_speed   — write overrides VSP ADC pot */
    setParamVariable(s, 0x40, UI_NONE,
                     (void*)&Motor.USER.u16VSP_filt_value,
                     sizeof(Motor.USER.u16VSP_filt_value));

    /* 0x41 direction      — CW=1, CCW=0 */
    setParamVariable(s, 0x41, UI_NONE,
                     (void*)&Motor.BLDC.u8Direction,
                     sizeof(Motor.BLDC.u8Direction));

    /* 0x42 run_enable_alt — duplicate of 0x09 standard; here for SNR-tool convenience */
    setParamVariable(s, 0x42, UI_NONE,
                     (void*)&Motor.BLDC.u8FlagEnMotorRun,
                     sizeof(Motor.BLDC.u8FlagEnMotorRun));

    /* 0x43 speed_rpm      — filtered electrical RPM (read-only convention) */
    setParamVariable(s, 0x43, UI_NONE,
                     (void*)&Motor.Control.u32MotorEleRPM_Final,
                     sizeof(Motor.Control.u32MotorEleRPM_Final));

    /* 0x44 bus_voltage_adc — raw 16-bit ADC */
    setParamVariable(s, 0x44, UI_NONE,
                     (void*)&User_sys.BLDC_SensorlessCtr.nBUS_Vol_ADC,
                     sizeof(User_sys.BLDC_SensorlessCtr.nBUS_Vol_ADC));

    /* 0x45 bus_current    — filtered */
    setParamVariable(s, 0x45, UI_NONE,
                     (void*)&User_sys.BLDC_SensorlessCtr.u32Ibus_Final,
                     sizeof(User_sys.BLDC_SensorlessCtr.u32Ibus_Final));

    /* 0x46 fault_flags    — write 0 to clear; bitfield per Global_Variable.h */
    setParamVariable(s, 0x46, UI_NONE,
                     (void*)&User_sys.BLDC_SensorlessCtr.sys_error_flg,
                     sizeof(User_sys.BLDC_SensorlessCtr.sys_error_flg));

    /* 0x47 mos_temp_adc */
    setParamVariable(s, 0x47, UI_NONE,
                     (void*)&User_sys.BLDC_SensorlessCtr.nMOS_NTC_ADC,
                     sizeof(User_sys.BLDC_SensorlessCtr.nMOS_NTC_ADC));

    /* 0x48 main_state     — m1m1_app_mainstate_t in M1_StateMachine.h */
    setParamVariable(s, 0x48, UI_NONE,
                     (void*)&e1M1_MainState,
                     sizeof(e1M1_MainState));

    /* 0x49 run_substate   — m1m1_run_substate_t */
    setParamVariable(s, 0x49, UI_NONE,
                     (void*)&e1M1_RunSubState,
                     sizeof(e1M1_RunSubState));
}

#endif /* MODULE_CODES_SNR */

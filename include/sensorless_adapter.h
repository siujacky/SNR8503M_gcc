#ifndef SENSORLESS_ADAPTER_H
#define SENSORLESS_ADAPTER_H

#include "feature_config.h"

#if MODULE_MOTOR_SENSORLESS

/* Phase 13: Motor_step_tab_cw/ccw exist in BOTH SDKs with different sizes.
 *
 * FIX2-004: rename macros are now OPT-IN via SENSORLESS_ADAPTER_RENAME_TABLES.
 * src/COMP_Sensoeless.c defines that token before including this header so
 * the sensorless [6] tables get prefixed names; Hall files that may include
 * sensorless_adapter.h (for diagnostic param registrations etc.) see the
 * original symbols and stay safe. */
#if MODULE_MOTOR_HALL && defined(SENSORLESS_ADAPTER_RENAME_TABLES)
#define Motor_step_tab_cw   sns_Motor_step_tab_cw
#define Motor_step_tab_ccw  sns_Motor_step_tab_ccw
#endif

#include <stdint.h>
#include "basic.h"

/*
 * Phase 13 — Sensorless adapter layer.
 *
 * The vendor sensorless SDK uses its own motor_structure typedef that
 * differs from our Hall version. Rather than refactor both, we provide
 * a parallel set of globals (Motor_1M1st, CMP_ZeroCross_lib) and let
 * the sensorless code work on its own state. Hall code stays untouched.
 *
 * Mode selection is fixed at boot; both globals exist but only one
 * is actively updated based on g_motor_mode.
 */

/* ------------------------------------------------------------------ */
/*  Sensorless BLDC struct — mirrors the SDK's BLDC_TypeDef            */
/* ------------------------------------------------------------------ */
typedef struct {
    __IO uint8_t  u8Direction;
    __IO uint8_t  u8FlagEnMotorRun;
    __IO uint8_t  u8FlagMotorIsRunning;
    __IO uint8_t  u8_ControlStep;
    __IO uint16_t u16_TimCnt;
} BLDC_SNS_TypeDef;

/* ------------------------------------------------------------------ */
/*  Sensorless BLDC_CMP — mirrors the SDK's BLDC_CMP_TypeDef           */
/* ------------------------------------------------------------------ */
typedef struct {
    __IO uint32_t u32COMP_OUT_temp;
    __IO uint32_t u32phase_cnt;

    __IO uint8_t  u8ControlStep;
    __IO uint8_t  u8ramp_cnt;
    __IO uint32_t u32RampTime;

    __IO uint32_t u32ElectricalStepValue;
    __IO uint32_t u32ElectricalStepcnt;
    __IO uint32_t u32ElectricalPeriod;
    __IO uint32_t u32ElectricalPeriodcnt;

    __IO uint8_t  u8FlagFreewheelAvoiding;
    __IO uint32_t u32FreewheelAvoidcnt;
    __IO uint32_t u32FreewheelAvoidDey;

    __IO uint32_t u32closeloop_count;
    __IO uint32_t u32MotorRotorBlock_cnt;

    __IO uint32_t u32MotorElePhaseValue[6];
    __IO uint32_t u32MotorElePeriodValue;
    __IO uint32_t u32MotorRPM;
    __IO uint32_t u32MotorRPMFilte_cnt;

    __IO uint32_t u32MotorElePhaseCnt[7];
    __IO uint32_t u32MotorElePhaseAvg;
    __IO uint32_t u32MotorElePhaseAvg_2;
    __IO uint32_t u32MotorElePhaseAvg_1_2;
    __IO uint32_t u32MotorElePhaseAvg_1_4;
    __IO uint32_t u32MotorElePhaseAvg_3_2;
    __IO uint8_t  u8FlagGetMotorElePeriod;
    __IO uint16_t u16MotorShakeCnt;
    __IO uint16_t u16MotorShakeDelayProtectCnt;

    __IO uint8_t  u8FlagDetectMotorFreerunning;
    __IO uint8_t  u8FlagDetectMotorFreerunTailwind;
    __IO uint8_t  u8FlagDetectMotorFreerunAgainstwind;
    __IO uint8_t  u8FlagDetectMotorFreerunnfinish;
    __IO uint8_t  u8Freerunstepchangecnt;
    __IO uint8_t  u8FreerunstepOld;
    __IO uint32_t u32DetectFreerunTimecnt;
    __IO uint32_t u32speed_cnt_temp;
    __IO uint32_t u32MotorFreerunAgainstwindDelay;

    __IO uint32_t u32ChargeTime_count;
} BLDC_SNS_CMP_TypeDef;

/* ------------------------------------------------------------------ */
/*  Sensorless USER (subset — same as Hall's USER for compat)          */
/* ------------------------------------------------------------------ */
typedef struct {
    __IO uint16_t u16VSP_filt_value;
    __IO uint16_t SpeedPI_Prc;
} USER_SNS_TypeDef;

/* The sensorless code references Motor_1M1st.{USER,BLDC,BLDC_CMP}.
 * Define a parallel "Motor_TypeDef" struct (different fields from Hall's). */
typedef struct {
    USER_SNS_TypeDef     USER;
    BLDC_SNS_TypeDef     BLDC;
    BLDC_SNS_CMP_TypeDef BLDC_CMP;
} Motor_SNS_TypeDef;

/* The SDK uses the type name "Motor_TypeDef" for this. Provide a
 * #define alias so the SDK source compiles unchanged when this header
 * is included BEFORE the SDK's motor_structure.h is included.
 * (We rely on the C preprocessor — Hall's Motor_TypeDef is left alone
 * because the SDK headers in src/COMP_Sensoeless.c and Find_Motor_Rotor.c
 * include OUR include/motor_structure.h via MC_Parameter.h transitively.) */

extern Motor_SNS_TypeDef Motor_1M1st;

/* Find_Motor_Rotor.c (excluded by default) references Motor_1st — same type. */
extern Motor_SNS_TypeDef Motor_1st;

/* CMP_Zero_TypeDef + CMP_ZeroCross_lib come from Lib_Zero_detect.h.
 * COMP_Motor_Zero_Detect, Freerun_Zero_Detect, CMP_SNS_Motor_*, etc.
 * come from COMP_Sensorless.h. We deliberately don't re-declare here. */
#include "Lib_Zero_detect.h"
#include "COMP_Sensorless.h"

/* Closed-lib renamed entry point (BLDC_init clash with Hall lib resolved
 * by objcopy in build.sh) */
extern void BLDC_init_snls(void);

/* FIX-006: HPWM_LON state constants for BLDC_COMP_Input_Only.
 * Values copied verbatim from sensorless SDK hardware_config.h:46-51.
 * These are commutation phase codes (which winding is high, which is low,
 * which is PWMed) used by BLDC_COMP_Input_Only to choose which BEMF line
 * goes to CMP0's positive input. */
#define VH_WL_HPWM_LON  6   /* A float | B PWM | C ON */
#define VH_UL_HPWM_LON  2   /* A ON    | B PWM | C float */
#define WH_UL_HPWM_LON  3   /* A ON    | B float | C PWM */
#define WH_VL_HPWM_LON  1   /* A float | B ON  | C PWM */
#define UH_VL_HPWM_LON  5   /* A PWM   | B ON  | C float */
#define UH_WL_HPWM_LON  4   /* A PWM   | B float | C ON */

/* BLDC_COMP_Input_Only — routes CMP0 input to the floating phase for BEMF
 * zero-crossing detection. Defined in src/sensorless_adapter.c (FIX-006). */
void BLDC_COMP_Input_Only(unsigned char Hall_value);

/* Alias: sensorless code calls BLDC_Communication; our codebase has BLDC_Sensor_control */
#define BLDC_Communication(state)  BLDC_Sensor_control(state)

#endif /* MODULE_MOTOR_SENSORLESS */
#endif /* SENSORLESS_ADAPTER_H */

/*
 * sensorless_adapter.c — Phase 13 — Definitions for sensorless globals.
 *
 * These are zero-initialised by the C runtime in .bss. The closed lib's
 * BLDC_init_snls and the SDK's INIT_FreerunDetect populate them at boot.
 */

#include "MC_Parameter.h"

#if MODULE_MOTOR_SENSORLESS

#include "sensorless_adapter.h"

Motor_SNS_TypeDef Motor_1M1st;
Motor_SNS_TypeDef Motor_1st;
CMP_Zero_TypeDef  CMP_ZeroCross_lib;

/* FIX2-006: counter of BLDC_COMP_Input_Only calls with invalid Hall_value.
 * Exposed via diagnostic param 0xB6 (see protocol_params_flash.c). */
volatile uint32_t g_sensorless_bad_phase_count = 0;

/* FIX-006: BLDC_COMP_Input_Only routes CMP0 input to the floating phase
 * based on commutation state for BEMF zero-crossing detection.
 *
 * Ported from sensorless SDK Kernal_Source MC_Drive.c lines 90 to 138.
 * Hall_value is one of the UH/VH/WH HPWM_LON commutation phase codes
 * (numeric values 1-6 matching the Hall sensor pattern equivalents).
 *
 * For each phase, two of the three windings are driven (PWM-high plus steady-on)
 * and one is floating. The BEMF on the floating phase is what we route to
 * CMP0 for zero-crossing detection. CMP0_SELN_SET fixes the negative input
 * (Hall mid-rail); CMP_IP_A/B/C selects which BEMF line goes to the positive
 * input.
 */
void BLDC_COMP_Input_Only(unsigned char Hall_value)
{
    SYS_WR_PROTECT = 0x7a83;
    SYS_AFE_REG1 &= 0xf8f3;     /* Clear bits for CMP0 input mux */

    switch (Hall_value) {
        case VH_WL_HPWM_LON:    /* A float | B PWM | C ON — falling edge on A */
            SYS_AFE_REG1 |= (CMP0_SELN_SET << 2) | (CMP_IP_A << 8);
            break;
        case VH_UL_HPWM_LON:    /* A ON | B PWM | C float — rising edge on C */
            SYS_AFE_REG1 |= (CMP0_SELN_SET << 2) | (CMP_IP_C << 8);
            break;
        case WH_UL_HPWM_LON:    /* A ON | B float | C PWM — falling edge on B */
            SYS_AFE_REG1 |= (CMP0_SELN_SET << 2) | (CMP_IP_B << 8);
            break;
        case WH_VL_HPWM_LON:    /* A float | B ON | C PWM — rising edge on A */
            SYS_AFE_REG1 |= (CMP0_SELN_SET << 2) | (CMP_IP_A << 8);
            break;
        case UH_VL_HPWM_LON:    /* A PWM | B ON | C float — falling edge on C */
            SYS_AFE_REG1 |= (CMP0_SELN_SET << 2) | (CMP_IP_C << 8);
            break;
        case UH_WL_HPWM_LON:    /* A PWM | B float | C ON — rising edge on B */
            SYS_AFE_REG1 |= (CMP0_SELN_SET << 2) | (CMP_IP_B << 8);
            break;
        default:
            /* FIX2-006: count invalid phase values for diagnostics. CMP0 stays
             * cleared (disconnected) so BEMF tracking can't lock onto noise. */
            if (g_sensorless_bad_phase_count < 0xFFFFFFFFU) {
                g_sensorless_bad_phase_count++;
            }
            break;
    }
    SYS_WR_PROTECT = 0;
}

#endif /* MODULE_MOTOR_SENSORLESS */

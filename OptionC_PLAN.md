# Option C — Full Hoverboard Emulation Plan (with Option B as the on-ramp)

**Generated:** 2026-05-12
**Status:** Design doc — see `PLAN.md` for the executable step list and `STATUS.md` for current loop state.

This document is the **standalone reference** for the B → C roadmap. PLAN.md mirrors the executable steps in self-loop format; this file holds the rationale, struct definitions, and full code allocation table that you'd want when writing or porting host tools.

---

## What problem this solves

The first bipropellant integration registered 10 SNR-specific parameters at codes **0x01–0x0A**. Those codes collide with what hoverboard-aware bipropellant tools expect (left+right motor structs, position data, etc.). Two design goals here:

1. **Option B:** Make off-the-shelf bipropellant host tools usable for the basics (read battery/current, enable motor) by moving SNR-specific codes out of the conflict range and implementing the 2 standard codes that map cleanly to single-motor hardware.
2. **Option C:** Pretend the SNR8503M is a one-wheel hoverboard. Host tools written for hoverboards Just Work, with right-motor fields filled with zeros.

B is a 2-3 hour deliverable that delivers ~80% of practical value. C requires 21-32 hours but yields full ecosystem compatibility.

The path from B to C is **purely additive** — no parameter codes are rearranged after Phase 5.

---

## Design principles

### Principle 1 — Don't squat on hoverboard codes
SNR custom parameters live at `0x40–0x4F`. Hoverboard-standard codes (`0x01–0x07, 0x0D, 0x80+`) stay free in Option B so Option C can claim them later without breakage.

### Principle 2 — Full struct shapes from day one
Where Option B implements a hoverboard standard code (0x08), it uses the **full** `PROTOCOL_ELECTRICAL_PARAMS` struct, with `motors[1]` zeroed. Upgrading to C just stops zeroing — no struct redefinition, no host re-pinning.

### Principle 3 — Pure-additive across phases
New files per phase, existing files (Phase 1+3) touched only once (Phase 5.1). Trying to revise a parameter handler in-place across multiple phases is the #1 source of regression bugs in incremental embedded ports.

### Principle 4 — Infrastructure now, payoff later
Phase 5.2 wires a Hall-tick accumulator that's barely used in B. C consumes it in 3 different parameter handlers without further changes. Same for `WHEEL_RADIUS_MM` and `HALL_TICKS_PER_REV` config constants added in 5.6.

---

## Code allocation reference

The complete map of who-owns-what across phases:

| Code | Phase added | Struct | R/W | Backing on SNR8503M | Notes |
|---|---|---|---|---|---|
| **0x01** | C-tier-2 (7.2) | `PROTOCOL_SENSOR_FRAME` | R | Synthesise `Angle` from VSP pot | Pretends VSP is a steering joystick |
| **0x02** | C-tier-1 (6.1) | `PROTOCOL_HALL_DATA_STRUCT` | R | `g_hall_ticks_abs`, `u32MotorEleRPM_Final`, `u32MotorElePeriodValue` | |
| **0x03** | C-tier-2 (7.2) | `PROTOCOL_SPEED_DATA` | RW | `wanted_speed_mm_per_sec[0]` ↔ VSP filt value | Switches to MODE_SPEED_CMD on write |
| **0x04** | C-tier-1 (6.1) | `PROTOCOL_POSN` | R | `g_hall_ticks_abs`, right=0 | |
| **0x05** | C-tier-2 (7.3) | `PROTOCOL_POSN_INCR` | W | Sets `target_ticks` for position-control mode | Engages MODE_POSITION |
| **0x06** | C-tier-2 (7.3) | `PROTOCOL_POSN_DATA` | RW | Position-mode config + telemetry | |
| **0x07** | C-tier-1 (6.1) | RawPosition (4×i32) | R | `g_hall_ticks_abs` + offset | |
| **0x08** | **B (5.3)** | `PROTOCOL_ELECTRICAL_PARAMS` | R | `nBUS_Vol_ADC`, `u32Ibus_Final`, `nMOS_NTC_ADC`; `motors[0]` filled, `motors[1]` zeros | Hoverboard battery monitors work |
| **0x09** | **B (5.4)** | u8 | RW | `Motor.BLDC.u8FlagEnMotorRun` | Drop-in motor enable |
| **0x0A** | **B (5.5)** | u8 | RW | Module-local stub | Hoverboard "disable poweroff" — no-op |
| **0x0B** | **B (5.5)** | u8 | RW | Module-local stub (could toggle RTT verbosity) | |
| **0x0C** | C-tier-1 (6.1) | `PROTOCOL_INTEGER_XYT_POSN` | R | `x = ticks * (2π·R_mm / HALL_TICKS_PER_REV)`; y, theta = 0 | Single-track dead reckoning |
| **0x0D** | C-tier-2 (7.2) | `PROTOCOL_PWM_DATA` | RW | `pwm[0]` ↔ direct `MCPWM_TH*` write | Engages MODE_PWM_DIRECT |
| **0x0E** | C-tier-2 (7.2) | `pwm` field only | RW | Subset of 0x0D | |
| **0x21** | **B (5.5)** | `PROTOCOL_BUZZER_DATA` | RW | LED blink as buzzer surrogate | |
| **0x40-0x49** | **B (5.1)** | individual variables | mostly RW | SNR-specific (was 0x01-0x0A in pre-Option-B build) | |
| **0x80** | C-tier-3 (8.3) | u32 magic | R | Flash config validation | |
| **0x81-0x83** | C-tier-3 (8.3) | u32 | RW | Position PID gains | Used by Phase 7 position loop |
| **0x84** | C-tier-3 (8.3) | u32 | RW | PositionPWMLimit | |
| **0x85-0x87** | C-tier-3 (8.3) | u32 | RW | Speed PID gains | **Replaces** `SSum_Kp/Ki/Kc` #defines |
| **0x88** | C-tier-3 (8.3) | u32 | RW | SpeedPWMIncrementLimit | Was `VSP_DUTY_ACC_LOAD` |
| **0x89** | C-tier-3 (8.3) | u32 (×100 amps) | RW | MaxCurrLim | **Replaces** `MAX_BUS_CURRENT_SETTINT` |
| **0x90** | C-tier-3 (8.3) | `PROTOCOL_ADC_SETTINGS` (~60B) | RW | Mostly no-op but persisted | Hoverboard ADC steering — N/A on our hardware |
| **0xA0** | C-tier-3 (8.3) | u8 | RW | HoverboardEnable feature gate | |

### SNR custom codes 0x40-0x49 in detail (Phase 5.1)

| Code | Name | Width | Direction | Backing | Notes |
|---|---|---|---|---|---|
| 0x40 | target_speed | u16 | RW | `Motor.USER.u16VSP_filt_value` | Override VSP pot |
| 0x41 | direction | u8 | RW | `Motor.BLDC.u8Direction` | CW=1, CCW=0 |
| 0x42 | run_enable_alt | u8 | RW | `Motor.BLDC.u8FlagEnMotorRun` | Duplicate of 0x09 for SNR-tool convenience |
| 0x43 | speed_rpm | u32 | R | `Motor.Control.u32MotorEleRPM_Final` | Filtered electrical RPM |
| 0x44 | bus_voltage_adc | u16 | R | `User_sys.BLDC_SensorlessCtr.nBUS_Vol_ADC` | Raw ADC |
| 0x45 | bus_current | u32 | R | `User_sys.BLDC_SensorlessCtr.u32Ibus_Final` | Filtered |
| 0x46 | fault_flags | u16 | RW | `User_sys.BLDC_SensorlessCtr.sys_error_flg` | Write 0 to clear |
| 0x47 | mos_temp_adc | u16 | R | `User_sys.BLDC_SensorlessCtr.nMOS_NTC_ADC` | Raw ADC |
| 0x48 | main_state | u8 | R | `e1M1_MainState` | Fault/Init/Stop/Run |
| 0x49 | run_substate | u8 | R | `e1M1_RunSubState` | Calib/Ready/Freerun_Det/Align/Startup/Spin/Brake |

---

## Required hoverboard struct definitions

These are the structs we have to populate exactly. From `bipropellant-protocol/protocol.h`:

```c
typedef struct tag_PROTOCOL_HALL_DATA_STRUCT {
    int32_t HallPosn;
    int32_t HallSpeed;
    float   HallPosnMultiplier;
    int32_t HallPosn_lastread;
    int32_t HallPosn_mm;
    int32_t HallPosn_mm_lastread;
    int32_t HallSpeed_mm_per_s;
    uint32_t HallTimeDiff;
    uint32_t HallSkipped;
} PROTOCOL_HALL_DATA_STRUCT;

typedef struct tag_PROTOCOL_MOTOR_ELECTRICAL {
    float dcAmps;
    float dcAmpsAvgAcc;
    float dcAmpsAvg;
    int32_t r1;
    int32_t r2;
    int32_t q;
    int32_t dcAmpsx100;
    int32_t pwm_limiter;
    int32_t pwm_requested;
    int32_t pwm_actual;
    uint32_t limiter_count;
} PROTOCOL_MOTOR_ELECTRICAL;

typedef struct tag_PROTOCOL_ELECTRICAL_PARAMS {
    int32_t bat_raw;
    float   batteryVoltage;
    int32_t board_temp_raw;
    float   board_temp_filtered;
    float   board_temp_deg_c;
    int32_t charging;
    int32_t dcCurLim;
    int32_t dc_adc_limit;
    PROTOCOL_MOTOR_ELECTRICAL motors[2];
} PROTOCOL_ELECTRICAL_PARAMS;

typedef struct tag_PROTOCOL_SPEED_DATA {
    int32_t wanted_speed_mm_per_sec[2];
    int32_t speed_max_power;
    int32_t speed_min_power;
    int32_t speed_minimum_speed;
    int32_t speed_diff_mm_per_sec[2];
    int32_t speed_power_demand[2];
} PROTOCOL_SPEED_DATA;

typedef struct tag_PROTOCOL_PWM_DATA {
    int32_t pwm[2];
    int32_t speed_max_power;
    int32_t speed_min_power;
    int32_t speed_minimum_pwm;
} PROTOCOL_PWM_DATA;

typedef struct tag_PROTOCOL_POSN {
    int32_t LeftAbsolute;
    int32_t RightAbsolute;
    int32_t LeftOffset;
    int32_t RightOffset;
} PROTOCOL_POSN;

typedef struct tag_PROTOCOL_POSN_INCR {
    int32_t Left;
    int32_t Right;
} PROTOCOL_POSN_INCR;

typedef struct tag_PROTOCOL_POSN_DATA {
    int32_t wanted_posn_mm[2];
    int32_t posn_max_speed;
    int32_t posn_min_speed;
    int32_t posn_diff_mm[2];
    int32_t posn_speed_demand[2];
} PROTOCOL_POSN_DATA;

typedef struct PROTOCOL_INTEGER_XYT_POSN_tag {
    int32_t x;
    int32_t y;
    int32_t degrees;
} PROTOCOL_INTEGER_XYT_POSN;

typedef struct tag_PROTOCOL_BUZZER_DATA {
    uint8_t  buzzerFreq;
    uint8_t  buzzerPattern;
    uint16_t buzzerLen;
} PROTOCOL_BUZZER_DATA;

typedef struct tag_PROTOCOL_SENSOR_FRAME {
    unsigned char header_00;
    short         Angle;
    short         Angle_duplicate;
    unsigned char AA_55;
    unsigned char Accelleration;
    unsigned char Accelleration_duplicate;
    short         Roll;
} PROTOCOL_SENSOR_FRAME;
```

---

## Unit conversions you'll need

These appear in multiple phases — define once.

```c
/* Voltage: raw ADC → volts */
#define VBUS_TO_VOLTS(adc) ((float)(adc) * (3.6f / 32752.0f) * VOLTAGE_SHUNT_RATIO)

/* Current: raw ADC → amps */
#define IBUS_TO_AMPS(adc)  ((float)(adc) / CURRENT_ADC_PER_A)

/* Hall ticks ↔ millimetres (forward distance) */
#define TICKS_TO_MM(t)     ((int32_t)((t) * (2.0f * 3.14159f * WHEEL_RADIUS_MM) / HALL_TICKS_PER_REV))
#define MM_TO_TICKS(mm)    ((int32_t)((mm) * HALL_TICKS_PER_REV / (2.0f * 3.14159f * WHEEL_RADIUS_MM)))

/* RPM ↔ mm/s */
#define RPM_TO_MM_PER_S(rpm) (((rpm) * 2.0f * 3.14159f * WHEEL_RADIUS_MM) / (60.0f * MOTOR_POLES))
#define MM_PER_S_TO_RPM(mms) (((mms) * 60.0f * MOTOR_POLES) / (2.0f * 3.14159f * WHEEL_RADIUS_MM))

/* PID gain Q15 ↔ hoverboard ×100 scaling */
#define Q15_TO_X100(q)     ((int32_t)((q) * 100.0f / 32768.0f))
#define X100_TO_Q15(x)     ((int32_t)((x) * 32768.0f / 100.0f))
```

These will land in `include/protocol_conv.h` during Phase 5.6 (the macros that use `WHEEL_RADIUS_MM` will be defined-but-unused until Phase 6).

---

## File-by-file layout (final, after Phase 8)

```
src/
├── (existing — Phase 1+3)
│   ├── main.c, interrupt.c, M1_StateMachine.c, MC_Drive.c, ...
│   ├── timebase.c                 (ms tick — Phase 3.2)
│   └── protocol_handlers.c        (callbacks + setup orchestration — Phase 3.3)
│
├── (Phase 5 — Option B)
│   ├── protocol_params_snr.c      (0x40-0x49)
│   ├── protocol_params_standard.c (0x08, 0x09, 0x0A, 0x0B, 0x21)
│   └── hall_accumulator.c
│
├── (Phase 6 — C tier 1)
│   └── protocol_params_position.c (0x02, 0x04, 0x07, 0x0C)
│
├── (Phase 7 — C tier 2)
│   ├── protocol_params_motion.c   (0x01, 0x03, 0x0D, 0x0E)
│   ├── control_mode.c             (mode dispatch)
│   └── position_control.c         (PI loop, 0x05+0x06)
│
└── (Phase 8 — C tier 3)
    ├── flash_config.c             (FlashContent_t + save/load)
    └── protocol_params_flash.c    (0x80-0xA0)

include/
├── (existing)
├── hall_accumulator.h, control_mode.h, position_control.h, flash_config.h
└── protocol_conv.h                (unit conversion macros)

docs/
├── port_notes.md                  (ARMCC→GCC migration notes, Phase 1.X)
├── protocol_codes.md              (parameter reference for host writers, Phase 5.8)
└── flash_layout.md                (Phase 8 — sector reservation, magic, defaults)
```

---

## Effort summary

| Phase | What | Hours | Cumulative |
|---|---|---|---|
| 5 | Option B | 2-3 | 2-3 |
| 6 | C tier 1 — position telemetry | 3-4 | 5-7 |
| 7 | C tier 2 — write cmds + control modes | 10-15 | 15-22 |
| 8 | C tier 3 — flash-backed config | 6-10 | 21-32 |

**Option B alone is enough** if you only need hoverboard tools to read battery and enable motor. Most users probably stop there.

**Option C tier 1** is the highest-value cheap upgrade — adds telemetry that makes the motor visible to Web Serial dashboards etc.

**Option C tier 2** is the highest-cost step — introduces new control modes that need careful hardware verification.

**Option C tier 3** is the highest-impact-on-existing-code step — touches PID/Protect/Ramp source files to convert `#define` → runtime variables. Worth doing only if you actually plan to tune PID at runtime from a host tool.

---

## When to stop

If you only need to "control this motor over UART", stop at **end of Phase 5**. Total commitment: ~3 hours from current state. Standard bipropellant tools see battery + motor enable; SNR custom codes 0x40-0x49 do the rest.

If you want bipropellant Python dashboards (`bipropellant-pyshell`, `bipropellant-webdashboard`) to plot motor data, complete **Phase 6**. Total commitment: ~7 hours from current state.

If you want a "this is a hoverboard" drop-in replacement for development boards, complete **Phase 7+8**. Total commitment: ~30 hours from current state. At that point you have a fully tunable, persistent-config motor controller that any hoverboard tool understands.

---

## Linked documents

- **PLAN.md** — executable step list for self-loop
- **STATUS.md** — current loop state (`current_step`, `next_step`, blocked reason)
- **BUILD_LOG.md** — append-only history of loop iterations
- **docs/protocol_codes.md** *(written end of Phase 5)* — parameter reference for host implementers

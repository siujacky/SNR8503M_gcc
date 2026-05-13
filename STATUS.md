# Loop Execution Status — Full (Phase 7+8) Branch

> Forked from `C:\RC\SNR8503M_gcc\` at end-of-Phase-6.1 (assumed verified good).
> All Phase 7+8 code lives in THIS folder; original folder stays untouched.

## Current

- **status:** `BLOCKED`
- **current_phase:** `13`
- **current_step:** `13.4_hardware_test`
- **next_step:** `Phase 14 polish (after hardware verify)`
- **last_result:** `Phase 13.1-13.3 done. ALL 7 PROFILES BUILD CLEAN. Sensorless adapter (Motor_1M1st, CMP_ZeroCross_lib, BLDC_COMP_Input_Only stub) in place. State machine dispatches on g_motor_mode. CMP_IRQHandler routes BEMF to closed-lib. Hall-only paths gated; sensorless mode swaps BLDC_init→BLDC_init_snls.`
- **last_updated:** `2026-05-12T07:30:00Z`
- **blocked_reason:** `needs_hardware. Sensorless mode builds but BLDC_COMP_Input_Only is currently a stub — motor won't commutate correctly via BEMF until the SDK's switch-statement implementation is ported. Hall mode is unchanged and verified-good.`
- **motor_runs:** `true (assuming Phase 1-6 verified good on original branch)`

## Phase progress

- [x] Phase 1-6 — assumed verified good on the parent branch (`C:\RC\SNR8503M_gcc\`)
- [x] **Phase 7 — write commands + new control modes** ✅
  - [x] 7.1 control_mode.{c,h} — dispatch layer; default MODE_VSP preserves prior behavior
  - [x] 7.2 protocol_params_motion.c — 0x01, 0x03, 0x0D, 0x0E
  - [x] 7.3 protocol_params_motion_pos.c + position_control.{c,h} — 0x05, 0x06
  - [x] 7.4 — DESIGN CHANGE: implemented at dispatch layer, no new M1_RunSubState. M1_StateMachine.c untouched.
- [x] **Phase 8 — flash-backed config** ✅ (partial 8.4)
  - [x] 8.1 linker.ld reserves 4 KB sector at 0x3F000 (FLASH_CONFIG region)
  - [x] 8.2 flash_config.{c,h} + FlashContent_t struct + load/save with 60s debounce
  - [x] 8.3 protocol_params_flash.c — 0x80-0xA0 registered (12 codes)
  - [ ] 8.4 PARTIAL — Position PID 0x81-0x84 LIVE; Speed PID + current-limit refactor deferred
  - [ ] 8.5 Hardware power-cycle persistence test (needs board)

## 27 registered codes (vs Phase 6.1's 14)

| Code | Phase | Direction | Description |
|---|---|---|---|
| 0x01 | 7.2 | R | sensor_copy (synth from VSP) |
| 0x02 | 6.1 | R | HallData |
| 0x03 | 7.2 | RW | SpeedData (engages MODE_SPEED_CMD on write) |
| 0x04 | 6.1 | R | Position |
| 0x05 | 7.3 | W | PositionIncr (engages MODE_POSITION) |
| 0x06 | 7.3 | RW | PosnData (write sets absolute target) |
| 0x07 | 6.1 | RW | RawPosition (W sets new zero offset) |
| 0x08 | 5.3 | R | electrical_measurements |
| 0x09 | 5.4 | RW | protocol_enable |
| 0x0A | 5.5 | RW | disablepoweroff (stub) |
| 0x0B | 5.5 | RW | debug_out (stub) |
| 0x0C | 6.1 | R | xytPosn |
| 0x0D | 7.2 | RW | PWMData (engages MODE_PWM_DIRECT) |
| 0x0E | 7.2 | RW | PWMData.pwm (subset) |
| 0x21 | 5.5 | RW | BuzzerData (LED surrogate) |
| 0x40-0x49 | 5.1 | mixed | SNR custom params (10 codes) |
| 0x80 | 8.3 | RW | FlashContent.magic |
| 0x81 | 8.3 | RW | PositionKpx100 (LIVE — Phase 7.3 reads) |
| 0x82 | 8.3 | RW | PositionKix100 (LIVE) |
| 0x83 | 8.3 | RW | PositionKdx100 (LIVE) |
| 0x84 | 8.3 | RW | PositionPWMLimit (LIVE) |
| 0x85 | 8.3 | RW | SpeedKpx100 (mirror-only) |
| 0x86 | 8.3 | RW | SpeedKix100 (mirror-only) |
| 0x87 | 8.3 | RW | SpeedKdx100 (mirror-only) |
| 0x88 | 8.3 | RW | SpeedPWMIncrementLimit (mirror-only) |
| 0x89 | 8.3 | RW | MaxCurrLim (mirror-only) |
| 0x90 | 8.3 | RW | ADCSettings (mirror-only) |
| 0xA0 | 8.3 | RW | HoverboardEnable (mirror-only) |

## Files added on this branch (relative to SNR8503M_gcc)

### src/
- `control_mode.c` (Phase 7.1)
- `position_control.c` (Phase 7.3)
- `flash_config.c` (Phase 8.2)
- `protocol_params_motion.c` (Phase 7.2)
- `protocol_params_motion_pos.c` (Phase 7.3)
- `protocol_params_flash.c` (Phase 8.3)

### include/
- `control_mode.h`
- `position_control.h`
- `flash_config.h`

### Modified
- `linker.ld` — reserved FLASH_CONFIG region at 0x3F000-0x3FFFF
- `src/main.c` — inline VSP read → `control_mode_dispatch()` + `flash_config_tick()` in 100 ms slot
- `src/protocol_handlers.c` — added 3 new `register_*()` calls + `position_control_init()` + `flash_config_load()`

### UNTOUCHED (design discipline preserved)
- `src/M1_StateMachine.c`, `src/MC_Drive.c`, `src/PID.c`, `src/Protect.c`, `src/SpeedRamp.c`, etc.
- All periph_driver/, startup/, third_party/, vendor_libs/

## Build artifacts

```
build/snr8503m.elf   ~248 KB (debug)
build/snr8503m.hex    ~43 KB (Intel HEX for flashing)
build/snr8503m.bin    ~17 KB (raw binary)
build/snr8503m.map   ~118 KB (link map)
```

Size: **text=28,380 / data=160 / bss=11,888** → **28.5 KB Flash, 12 KB RAM**. Under 32 KB MDK-Lite cap.

## Critical caveats (READ BEFORE FLASHING)

1. **Boot behavior unchanged.** Firmware comes up in MODE_VSP — VSP pot still drives speed. No surprise motion.
2. **Position control is untested.** The PI gains in `flash_config_set_defaults()` are bench placeholders (Kp=1.00 ×100). Will need tuning per motor.
3. **Flash sector 0x3F000-0x3FFFF will be erased on first write.** Initial boot: blank flash → restore defaults → mark dirty → save after 60 s debounce.
4. **Phase 8.4 INCOMPLETE.** Speed PID (0x85-0x87) and MaxCurrLim (0x89) writes update the flash mirror and host reads reflect them, BUT the existing speed loop continues to use compile-time `SSum_Kp/Ki/Kc` and `MAX_BUS_CURRENT_SETTINT`. Position PID (0x81-0x84) IS fully live.
5. **No FPU.** mm/s ↔ RPM conversions in 0x03/0x06 use soft-float. Avoid subscribing at 1 ms intervals.
6. **MODE_PWM_DIRECT** maps through VSP pipeline (safer; range capped at VSP_MAX_VALUE).
7. **No "return to MODE_VSP" command.** Reboot to reset mode. (Could add a code in a future phase.)

## Hardware verification plan

1. **Pre-flash:** keep `C:\RC\SNR8503M_gcc\build\snr8503m.hex` (Phase 6.1) as fallback.
2. Flash `C:\RC\SNR8503M_gcc_full\build\snr8503m.hex`. Boot identical to Phase 5+6.1 — VSP pot drives motor normally. No mode change without explicit host command.
3. `read 0x80` → 0xDEADBEEF; `read 0x81` → 100.
4. `write 0x81 = 50` → wait 65 s → power-cycle → `read 0x81` → 50 (Flash persisted).
5. `write 0x03 SpeedData(mm/s=200)` → motor spins at the corresponding RPM.
6. **CAREFUL:** `write 0x05 PositionIncr(Left=10mm)` → motor rotates ~10 mm of forward travel. Position PI gains may be wrong; expect oscillation or under-shoot until tuned.
7. `write 0x0D PWMData(pwm=500)` → low-duty spin.
8. Power-cycle to return to MODE_VSP.

## Notes

- If anything goes wrong, original `SNR8503M_gcc/` is intact for fallback.
- This branch represents the "full hoverboard emulation" surface. ~30 hours of design work compressed into the codebase.
- Phase 8.4 refactor is the only deferred item. Worth ~6-10 hr of follow-up work; not blocking host integration.

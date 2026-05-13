# SNR8503M GCC Firmware

A modular, GCC-buildable firmware for the **SNR8503M BLDC motor controller** (re-marked LKS32MC03x, ARM Cortex-M0 @ 48 MHz, 256 KB Flash, 128 KB RAM). Supports Hall-sensored and BEMF-sensorless commutation, with a pluggable serial protocol layer ranging from a 9-byte vendor stub to full bipropellant binary + ASCII.

**Status:** All 7 build profiles compile clean. Hardware bench-verification pending for sensorless mode.

---

## Quick start

```bash
# Default profile — Hall + bipropellant binary + ASCII + all codes + flash config
./build.sh build

# Smallest profile — Hall + 9-byte vendor protocol only
./build.sh rebuild PROFILE=hall_minimal

# Auto-detect Hall vs Sensorless at boot
./build.sh rebuild PROFILE=unified_full

# Output: build/snr8503m.{elf,hex,bin,map}
```

Then flash `build/snr8503m.hex` via J-Link, openocd, or the Keil flash algorithm at `SNR8503x.FLM` (in the parent `SNR8503M-Hall-main` directory).

---

## Why this firmware exists

The original vendor firmware was distributed as a Keil µVision project with two closed-source `.lib` files and assumed exclusive use of ARM Compiler 5. This project:

- Builds with **GNU Arm GCC** (no Keil dependency), but **still links the original vendor `.lib` files** unmodified
- Adds **bipropellant binary protocol** support for runtime control and telemetry over UART0
- Adds **sensorless (BEMF) commutation** as a compile-time option, with auto-detection at boot
- Splits the codebase into **11 modules** selectable via **7 named profiles**, so you can build anything from a 15 KB minimal stub to a 30 KB feature-complete firmware from a single source tree

---

## Profile system

Profiles are selected at build time via `PROFILE=<name>`. The build script regenerates `include/feature_config.h` from the selected preset.

| Profile | Flash | RAM | Description |
|---|---|---|---|
| `hall_minimal` | 14.9 KB | 1.5 KB | Hall + vendor 9-byte protocol. Smallest. |
| `hall_bipropellant` | 25.4 KB | 11 KB | Hall + bipropellant binary, no ASCII. |
| `hall_bipropellant_full` | 28.8 KB | 12 KB | **Default.** Hall + binary+ASCII + all code modules + Flash config. |
| `sensorless_minimal` | 15.3 KB | 1.6 KB | BEMF-sensorless + vendor protocol. For boards without Hall sensors. |
| `sensorless_bipropellant` | 25.2 KB | 11 KB | Sensorless + binary protocol. |
| `unified_minimal` | 16.5 KB | 1.7 KB | Both motor modes, auto-detect at boot, vendor protocol. |
| `unified_full` | 30.7 KB | 12 KB | Everything. Hall + Sensorless auto-detect + binary+ASCII + all codes. |

All profiles fit under the 32 KB MDK-Lite cap (so this firmware could also be built in Keil's free Community Edition with the original ARMCC 5 toolchain).

---

## Architecture

### High-level layers

```
┌──────────────────────────────────────────────────────────┐
│  bipropellant host (PC, Pi, BLE bridge)                  │
│   - reads/writes 32 parameter codes over UART0 @ 9600    │
└──────────────────────────────────────────────────────────┘
                              │ UART
┌──────────────────────────────────────────────────────────┐
│  Protocol layer (modular)                                │
│   • simple (UR_Ctrl.c — 9-byte AA…55 stub), OR           │
│   • bipropellant binary (third_party/), optionally + ASCII│
└──────────────────────────────────────────────────────────┘
                              │
┌──────────────────────────────────────────────────────────┐
│  Parameter handlers (modular)                            │
│   • standard codes 0x08, 0x09, 0x0A, 0x0B, 0x21          │
│   • SNR custom 0x40-0x49                                 │
│   • position 0x02, 0x04, 0x07, 0x0C                      │
│   • motion 0x01, 0x03, 0x05, 0x06, 0x0D, 0x0E            │
│   • flash config + diagnostics 0x80-0xA0, 0xB0-0xB6      │
└──────────────────────────────────────────────────────────┘
                              │
┌──────────────────────────────────────────────────────────┐
│  Control mode dispatch                                   │
│   MODE_VSP (default) | MODE_SPEED_CMD | MODE_PWM_DIRECT  │
│                                       | MODE_POSITION    │
└──────────────────────────────────────────────────────────┘
                              │
┌──────────────────────────────────────────────────────────┐
│  Motor state machine                                     │
│   Fault → Init → Stop → Run                              │
│     Run sub-states: Calib/Ready/Freerun/Align/Startup/   │
│                     Spin/Brake                           │
│   Dispatches on g_motor_mode (Hall or Sensorless)        │
└──────────────────────────────────────────────────────────┘
                              │
┌──────────────────────────────────────────────────────────┐
│  Commutation                                             │
│   Hall path: HALL_GetFilterValue + HALL_Update + BLDC_   │
│     Sensor_control                                       │
│   Sensorless: COMP_Motor_Zero_Detect via CMP0 IRQ +      │
│     BLDC_COMP_Input_Only routing                         │
└──────────────────────────────────────────────────────────┘
                              │
┌──────────────────────────────────────────────────────────┐
│  Vendor closed libs (linked unmodified)                  │
│   • SNR_BLDC_HALL_V1p0.lib (Hall: BLDC_init, HALL_Update,│
│     ICP_Loader)                                          │
│   • snr_bldc_snls_renamed.lib (Sensorless: BLDC_init_snls│
│     COMP_Motor_Zero_Detect, Freerun_Zero_Detect)         │
│   • snr8503x_nvr.lib (Read_Trim)                         │
└──────────────────────────────────────────────────────────┘
```

### Module catalog

11 modules selectable via `feature_config.h`:

| Module flag | Purpose |
|---|---|
| `MODULE_MOTOR_HALL` | Hall-sensored commutation path |
| `MODULE_MOTOR_SENSORLESS` | BEMF-comparator commutation path |
| `MODULE_PROTOCOL_SIMPLE` | Original vendor 9-byte protocol |
| `MODULE_PROTOCOL_BIPROPELLANT_BIN` | bipropellant binary protocol |
| `MODULE_PROTOCOL_BIPROPELLANT_ASCII` | bipropellant ASCII variant |
| `MODULE_CODES_STANDARD` | 0x08 electrical, 0x09 enable, etc. |
| `MODULE_CODES_SNR` | SNR custom params at 0x40-0x49 |
| `MODULE_CODES_POSITION` | Position telemetry 0x02/0x04/0x07/0x0C |
| `MODULE_CODES_MOTION` | Speed/PWM write commands |
| `MODULE_CODES_POSCTRL` | Position-control PI loop |
| `MODULE_FLASH_CONFIG` | Persistent runtime-tunable config |

Cross-module dependencies and mutual-exclusion rules are enforced at compile time via `#error` directives in `feature_config.h`.

---

## File layout

```
SNR8503M_gcc_full/
├── README.md                  ← this file
├── lesson-learn-and-review.html  ← project retrospective
├── PLAN.md                    ← self-loop execution log
├── STATUS.md                  ← current build state
├── BUILD_LOG.md               ← append-only history
├── OptionC_PLAN.md            ← Option C design rationale
├── hall-and-hall-less-plan.json  ← Hall+Sensorless integration plan
├── fix-up-plan.json           ← Code-review iteration 1 (24/25 fixed)
├── fix-up-plan-2.json         ← Code-review iteration 2 (13/13 fixed)
├── docs/
│   └── protocol_codes.md      ← Bipropellant code reference for host writers
├── build.sh                   ← Build script with PROFILE= support
├── Makefile                   ← (alternative — equivalent to build.sh)
├── linker.ld                  ← GCC linker script
├── startup/
│   ├── startup_snr8503x.s     ← Original armasm V5 (reference)
│   └── startup_snr8503x_gnu.S ← Converted GNU as syntax
├── src/                       ← Application code (per-module .c files)
├── include/                   ← Headers
├── periph_driver/             ← Vendor peripheral drivers (LKS / SNR rebrand)
├── third_party/
│   └── bipropellant-protocol/ ← MIT, verbatim from upstream
└── vendor_libs/
    ├── SNR_BLDC_HALL_V1p0.lib       ← Hall motor lib (proprietary)
    ├── snr8503x_nvr.lib             ← NVR access lib (proprietary)
    ├── SNR_BLDC_SNLS_V1p0.lib       ← Sensorless lib (proprietary)
    └── snr_bldc_snls_renamed.lib    ← Auto-generated rename of above (BLDC_init → BLDC_init_snls)
```

---

## Hardware setup

- **MCU**: SNR8503M (= LKS32MC03x), ARM Cortex-M0 at 48 MHz
- **Programmer**: J-Link, ULINK, or CMSIS-DAP via SWD
- **UART for protocol**: P1.6 (TX) / P1.7 (RX), 9600 baud 8N1
- **Motor**: 6-80V BLDC with 3-phase inverter (board ships with 20A inverter)
- **Hall sensors**: 3 inputs on the standard SNR8503M board (auto-detected at boot)
- **Power**: 6-80V DC at the bus terminals

See `SNR8503M_BLDC_Hall_6-80V_20A_V2.0.SchDoc` (in the parent directory) for the full schematic.

---

## Bipropellant protocol — quick reference

When using a profile with `MODULE_PROTOCOL_BIPROPELLANT_BIN=1`, the host can read/write these codes via UART0 @ 9600 baud. See `docs/protocol_codes.md` for the full reference and timing budget.

### Most-used codes

| Code | R/W | Type | Purpose |
|---|---|---|---|
| `0x08` | R | struct | Electrical measurements: battery voltage, current, motor temp |
| `0x09` | RW | u8 | Motor enable (0 = stop, 1 = run) |
| `0x03` | RW | struct | Speed command in mm/s (engages MODE_SPEED_CMD) |
| `0x04` | R | struct | Position (Hall ticks since boot) |
| `0x46` | RW | u16 | Fault flags bitfield (write 0 to clear) |
| `0xB0` | R | u8 | Active motor mode (0 = Hall, 1 = Sensorless) |
| `0xB1` | RW | u8 | Mode override (0 = auto, 1 = force Hall, 2 = force Sensorless) — write + reboot |
| `0xB4` | R | u32 | **Firmware features bitfield — tells host what's compiled in** |

### Diagnostic codes (post-fix-up iteration 2)

| Code | Purpose |
|---|---|
| `0xB4` | Build-time module flags (12 bits) |
| `0xB5` | Flash save failure counter |
| `0xB6` | Sensorless invalid-phase count |

---

## Build details

### Toolchain auto-detection (FIX-014 / FIX2-008)

`build.sh` checks for the ARM GCC toolchain in this order:

1. `$ARM_TOOLCHAIN/arm-none-eabi-gcc` (with `.exe` fallback for Windows MSYS)
2. `arm-none-eabi-gcc` on `$PATH`
3. PlatformIO's bundled toolchain at `~/.platformio/packages/toolchain-gccarmnoneeabi/bin`

If none are found, the build aborts with a clear message.

### Vendor lib symbol rename

When `MODULE_MOTOR_SENSORLESS=1`, `build.sh` runs:

```bash
arm-none-eabi-objcopy \
    --redefine-sym BLDC_init=BLDC_init_snls \
    --redefine-sym BLDC_1=BLDC_1_snls \
    --redefine-sym BLDC_BUFF=BLDC_BUFF_snls \
    vendor_libs/SNR_BLDC_SNLS_V1p0.lib \
    vendor_libs/snr_bldc_snls_renamed.lib
```

This sidesteps a symbol clash between the Hall and Sensorless vendor libs (both export `BLDC_init` and have data-section names `BLDC_1`/`BLDC_BUFF`).

### Linker quirk

ARMCC `.lib` files include some helper symbols (`__rt_*`, `__aeabi_*`) that GCC's libgcc may not fully match. The build adds `-Wl,--no-warn-mismatch` to suppress non-fatal warnings. So far no real runtime issues observed.

---

## Testing

### Build-time tests

- `_Static_assert` checks: `PWM_FREQ` divisible by 1000 (timebase.c), `sizeof(FlashContent_t) % 4 == 0` (flash_config.c)
- Compile-time module-constraint validation in `feature_config.h` (`#error` if violated)
- All 7 profiles must build without errors or warnings — verified manually after every change

### Boot-time tests

- `startup_canary_check()` at main() entry verifies that `.data` was copied correctly by Reset_Handler (guard against FIX-001 regression)
- Initial `motor_mode_detect()` reads Hall pins to choose mode
- `flash_config_load()` verifies magic + version + CRC32

### Runtime smoke tests (require hardware)

1. Read `0x08` — should return a struct with `batteryVoltage` ≈ your bus voltage (float)
2. Read `0x0B4` — should return a 32-bit bitfield matching the built profile's module flags
3. Read `0x09` — should return 0 (motor stopped at boot)
4. Write `0x09 = 1`, `0x40 = 1500` — motor should spin
5. Read `0x04` — `LeftAbsolute` should change with motor rotation
6. Write `0x46 = 0` — clears any fault flags
7. Read `0xB0` — reports active mode (Hall or Sensorless)

---

## Known issues & limitations

| Issue | Mitigation |
|---|---|
| **`BLDC_COMP_Input_Only` bit positions unverified on hardware.** Ported verbatim from sensorless SDK MC_Drive.c:90. SYS_AFE_REG1 bit positions assumed correct for this board variant. | Bench-verify before relying on sensorless mode. |
| **Sensorless tuning constants are vendor defaults.** `MAX_SPEED_CNT=1500`, `MIN_SPEED_CNT=10`. Likely need tuning per motor. | Adjust in `MC_Parameter.h` under `#if MODULE_MOTOR_SENSORLESS`. |
| **`WHEEL_RADIUS_MM=50` is a placeholder.** Affects xytPosn accuracy. | Set to actual wheel radius before deploying. |
| **Phase 8.4 incomplete.** Speed PID (0x85-0x87) and current limit (0x89) writes persist to Flash but don't yet affect the running speed loop (which uses compile-time `#define`s). Position PID is fully runtime-tunable. | If you need runtime speed-PID tuning, refactor `PID.c` / `SpeedRamp.c` to read from `g_flash_cfg`. ~6-10 hours. |
| **Soft-float in protocol reads.** Cortex-M0 has no FPU. `0x08` electrical_measurements does ~4 µs of soft-float per read. | For tight loops, use raw ADC codes `0x44`/`0x45` instead of `0x08`. |
| **Cold-boot saves to blank Flash within 60s.** First boot finds blank Flash, restores defaults, writes them out. Subsequent boots load existing config. | Expected behavior. |
| **Upgrading from pre-FIX-013 firmware loses saved tunings.** `FLASH_CONFIG_VERSION` bumped from 1 to 2 when CRC was added. Old saved configs fail validation. | Re-save tunings after first boot of new firmware. |

---

## Project history

This firmware was incrementally built across 13 development phases plus 2 code-review fix cycles. See:

- `lesson-learn-and-review.html` — retrospective with what worked, what didn't, and metrics
- `PLAN.md` + `OptionC_PLAN.md` + `hall-and-hall-less-plan.json` — design plans and rationale
- `BUILD_LOG.md` — append-only log of every build iteration
- `fix-up-plan.json` + `fix-up-plan-2.json` — code review findings + fix status

---

## License

This firmware is a derivative work of multiple sources:

| Component | License |
|---|---|
| SNR8503M peripheral drivers (in `periph_driver/`) | Originally from LINKO SEMICONDUCTOR's BSD-3-Clause platform_software; re-branded by SNANER SEMICONDUCTOR. Effective: **BSD-3-Clause** |
| Original Phase 1-8 application code | Vendor source from SNANER, no explicit license. Used here as-is. |
| `third_party/bipropellant-protocol/` | **MIT** (preserved from upstream) |
| `vendor_libs/*.lib` | Proprietary (SNANER / LINKO); linked unmodified |
| New code (modular layer, sensorless adapter, build scripts, plans, docs) | Author chose to ship with no formal license declaration. Treat as BSD-equivalent for personal use; consult the author for any commercial / redistributable arrangement. |

The bipropellant-protocol's MIT license is permissive and compatible with linking against proprietary code. No GPL-style copyleft obligations apply.

---

## Acknowledgements

- LINKO Semiconductor / SNANER — for the SNR8503M / LKS32MC03x chip family and the original vendor SDK
- The bipropellant project — for the well-documented MIT-licensed binary protocol
- The PlatformIO project — for bundling a working `arm-none-eabi-gcc` toolchain that was used during development

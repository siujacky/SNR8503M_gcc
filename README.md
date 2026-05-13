# SNR8503M GCC Firmware

A modular, GCC-buildable firmware for the **SNR8503M BLDC motor controller** (re-marked LKS32MC03x, ARM Cortex-M0 @ 48 MHz, 256 KB Flash, 128 KB RAM). Supports Hall-sensored and BEMF-sensorless commutation, with a pluggable serial protocol layer ranging from a 9-byte vendor stub to full bipropellant binary + ASCII.

**Status:** All 7 build profiles compile clean. Hardware bench-verification pending for sensorless mode.

**License:** **GPL-3.0-or-later** — fully free/libre firmware. No proprietary `.lib` files are linked. See [LICENSE](LICENSE) for the full text and the [License](#license) section below for the per-component breakdown.

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

The original vendor firmware was distributed as a Keil µVision project with three closed-source `.lib` files (`SNR_BLDC_HALL_V1p0.lib`, `SNR_BLDC_SNLS_V1p0.lib`, `snr8503x_nvr.lib`) and assumed exclusive use of ARM Compiler 5. This project:

- Builds with **GNU Arm GCC** (no Keil dependency)
- **Builds entirely from open source** — the proprietary `.lib` files have been removed from the repository, and the few functions that used to depend on them are now either inlined directly into the firmware source or replaced with textbook BLDC algorithms (see [Public-domain algorithm references](#public-domain-algorithm-references) below)
- Adds **bipropellant binary protocol** support for runtime control and telemetry over UART0
- Adds **sensorless (BEMF) commutation** as a compile-time option, with auto-detection at boot
- Splits the codebase into **11 modules** selectable via **7 named profiles**, so you can build anything from a 15 KB minimal stub to a 30 KB feature-complete firmware from a single source tree

---

## Profile system

Profiles are selected at build time via `PROFILE=<name>`. The build script regenerates `include/feature_config.h` from the selected preset.

| Profile | Flash | RAM | Description |
|---|---|---|---|
| `hall_minimal` | 14.0 KB | 1.5 KB | Hall + 9-byte simple protocol. Smallest. |
| `hall_bipropellant` | 24.3 KB | 11 KB | Hall + bipropellant binary, no ASCII. |
| `hall_bipropellant_full` | 27.6 KB | 12 KB | **Default.** Hall + binary+ASCII + all code modules + Flash config. |
| `sensorless_minimal` | 14.7 KB | 1.6 KB | BEMF-sensorless + 9-byte simple protocol. For boards without Hall sensors. |
| `sensorless_bipropellant` | 24.5 KB | 11 KB | Sensorless + binary protocol. |
| `unified_minimal` | 15.5 KB | 1.7 KB | Both motor modes, auto-detect at boot, 9-byte simple protocol. |
| `unified_full` | 29.4 KB | 12 KB | Everything. Hall + Sensorless auto-detect + binary+ASCII + all codes. |

All profiles fit under the 32 KB MDK-Lite cap, so this firmware could also be built in Keil's free Community Edition with the original ARMCC 5 toolchain (though the build script targets GCC).

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
│  Low-level motor primitives (open-source, GPL-3.0)       │
│   • Hall path:    inlined Hall-value pickup              │
│                   (HALL_Update was an identity stub —    │
│                   dropped; real work in Sensor_Control.c)│
│   • Sensorless:   COMP_Motor_Zero_Detect (~13 lines),    │
│                   Freerun_Zero_Detect (~18 lines)        │
│                   in src/COMP_Sensoeless.c               │
│   • DAC trim:     identity defaults (DAC_AMC=512,        │
│                   DAC_DC=0) in periph_driver/.../dac.c — │
│                   no NVR access required                 │
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
├── LICENSE                    ← Full GPL-3.0 license text
├── README.md                  ← this file
├── lesson-learn-and-review.html  ← project retrospective
├── PLAN.md                    ← Phase-by-phase execution log
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
├── periph_driver/             ← Peripheral drivers (LINKO SEMICONDUCTOR BSD-3-Clause)
└── third_party/
    └── bipropellant-protocol/ ← MIT, verbatim from upstream
```

Note: the original SNANER/LINKO closed `.lib` files used to live under `vendor_libs/` in earlier versions of this repo. They are no longer present in the working tree — see the [Vendor archive](#vendor-archive) note in the Build details section below.

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

### Closed-library replacement

The original SNANER firmware linked three proprietary `.lib` files (`SNR_BLDC_HALL_V1p0.lib`, `SNR_BLDC_SNLS_V1p0.lib`, `snr8503x_nvr.lib`). Every symbol they exposed has either been removed from the firmware entirely (because nothing actually needed it) or replaced with an open-source equivalent inlined directly into the source. Detail per symbol:

| Original symbol | What it was | Fate in this codebase |
|---|---|---|
| `BLDC_init` / `BLDC_init_snls` | License-gate stubs (read NVR trim, compare, infinite-loop on mismatch) | **Calls removed** from `src/hardware_init.c`. The gates served no functional purpose. |
| `BLDC_1`, `BLDC_BUFF`, `BLDC_BUFF1` (+ `*_snls`) | License-gate globals | **Deleted entirely** — never read by anything outside the gate, pure dead state. |
| `HALL_Update(p)` | One-line identity passthrough that returned `p->u8HALL_NEW_Value` | **Inlined** at its single caller in `src/Sensor_Control.c`. |
| `ICP_Loader` | In-Circuit-Programming pulse-pattern detector on P0.4 | **Call removed** from TIMER0 IRQ in `src/interrupt.c`. We re-flash via SWD, not the vendor ICP path. |
| `ICP_MODE`, `ICP_SetFLAG`, 7 other `ICP_*CNT*` globals | Bootloader-entry state | **Deleted entirely** — never read by anything outside the bootloader-detect routine. |
| `Read_Trim(addr)` | NVR-read with proprietary unlock sequence (the magic constants `0x59000000`, `0x95000000`) | **Calls removed.** Its only caller (`periph_driver/source/snr8503x_dac.c`) was patched to use identity defaults (`DAC_AMC = 512`, `DAC_DC = 0`) so no NVR access is needed at all. |
| `COMP_Motor_Zero_Detect(p)` | BEMF zero-crossing → speed-counter adjustment | **Reimplemented in `src/COMP_Sensoeless.c`** as a ~13-line textbook BLDC algorithm (see [Public-domain algorithm references](#public-domain-algorithm-references)). |
| `Freerun_Zero_Detect(p)` | Freewheel-direction detector | **Reimplemented in `src/COMP_Sensoeless.c`** as a ~18-line standard debounce-then-commit pattern. |

Result: **every byte in the final ELF/HEX/BIN is built from freely-distributable source**. No ARMCC/proprietary symbol mismatches, no `--no-warn-mismatch` linker flag, no `.lib` files in the repo.

### Vendor archive

The original `.lib` files and the decompilation outputs used during this project are **not** in the public repo. They are kept locally by the project author in `VENDOR_ARCHIVE.zip` (referenced by `VENDOR_ARCHIVE_README.txt` in the same directory). Both files are excluded from git via `.git/info/exclude`. The archive contents are documented in `VENDOR_ARCHIVE_README.txt` for reference, but they are SNANER / LINKO SEMICONDUCTOR proprietary IP and must not be redistributed.

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

The combined firmware is distributed under **GPL-3.0-or-later**. See [`LICENSE`](LICENSE) for the full GNU General Public License v3 text.

### Component-by-component breakdown

| Component | License | GPL-3 compatible? |
|---|---|---|
| All project-authored files in `src/`, `include/`, `startup/`, `build.sh`, `linker.ld`, `Makefile`, all `*.md` / `*.html` / `*.json` documentation | **GPL-3.0-or-later** (this project) | — (this is the project license) |
| `periph_driver/` peripheral drivers | Originally from LINKO SEMICONDUCTOR's BSD-3-Clause platform_software; re-branded by SNANER SEMICONDUCTOR. Effective: **BSD-3-Clause** | Yes — BSD-3-Clause is GPL-compatible |
| `third_party/bipropellant-protocol/` | **MIT** (preserved verbatim from [bipropellant/bipropellant-protocol](https://github.com/bipropellant/bipropellant-protocol)) | Yes — MIT is GPL-compatible |
| Original SNANER reference firmware code that survives in `src/` (state machine, protection, PI loop, etc.) | **License never explicitly declared** by SNANER. Historically distributed by SNANER without any license header. Treated here as redistributable on the same basis SNANER published it. | See [License grey-zone](#license-grey-zone) below |

GPL-3.0 acts as the "ceiling" license: BSD-3-Clause and MIT code can be combined into a GPL-3.0 work, and the combined work as distributed is GPL-3.0. The MIT and BSD attributions are preserved in their respective source headers.

### License grey-zone

The largest remaining open IP item is the original SNANER reference firmware: files that SNANER published as part of their reference application (the motor-control state machine in `src/M1_StateMachine.c`, the protection logic, the original `src/main.c`, the PI controller in `src/PID.c`, the Hall-state lookup tables, etc.). SNANER never attached an explicit license to these. The community convention is to treat them as openly redistributable because that is how SNANER publishes them, but a strictly-correct GPL declaration would require either SNANER's explicit consent or a from-scratch rewrite of those files. Practical risk for non-commercial use is low.

### Anti-Tivoization

This firmware is GPL-3.0 specifically (not GPL-2.0). If you ship a hardware product with this firmware in it and lock the bootloader so that end users cannot install modified versions, you may be in breach of GPL-3.0 §6 (the "anti-Tivoization" clause). The SNR8503M flashes via SWD, which is normally user-accessible — preserving that path is the simplest way to stay compliant.

### A note on the BLDC algorithms

The two ~15-line BEMF zero-crossing helpers in `src/COMP_Sensoeless.c` (`COMP_Motor_Zero_Detect`, `Freerun_Zero_Detect`) implement well-documented BLDC patterns described in publicly-available semiconductor application notes (see [Public-domain algorithm references](#public-domain-algorithm-references)). Their *existence* as named entry points in this firmware came from observing what the original closed `.lib` files exported, but their *content* is textbook control-theory that appears in dozens of open-source BLDC drivers.

---

## Public-domain algorithm references

Every motor-control technique in this firmware is described in publicly-available semiconductor application notes. The list below credits the sources whose ideas appear (in some form) in the codebase. Where the original implementation came from a closed vendor lib, the citation is what would let an independent engineer re-derive the algorithm from first principles.

### BEMF zero-crossing speed regulation (sensorless commutation)

Implemented in `src/COMP_Sensoeless.c` as `COMP_Motor_Zero_Detect`. The technique — comparator-output polarity vs. expected edge polarity drives an up/down counter that adjusts the commutation period — is documented in:

| Reference | Authority | Topic |
|---|---|---|
| **SLVA372** | Texas Instruments | "InstaSPIN-BLDC: Sensorless BLDC Motor Solution" — the canonical BEMF zero-crossing speed-track algorithm |
| **SPRA420** | Texas Instruments | "Sensorless Control of Brushless DC Motors Using the TMS320 Family" — earlier formal treatment |
| **AN1944** | STMicroelectronics | "Sensorless 3-phase BLDC motor control with the STM32" |
| **AN3266** | NXP / Freescale | "Sensorless BLDC Motor Control with Kinetis" |
| **AN857** | Microchip | "Brushless DC Motor Control Made Easy" — accessible introduction |

### Freewheel direction detection (debounce-then-commit)

Implemented in `src/COMP_Sensoeless.c` as `Freerun_Zero_Detect`. The integrator-style pattern (per-state counters that increment on agreement and decrement on disagreement, with a commit threshold) is the standard denoising approach for noisy comparator edges. Documented in:

| Reference | Authority | Topic |
|---|---|---|
| **AVR443** | Atmel (now Microchip) | "Sensor-based control of three-phase brushless DC motor" — includes the freewheel-detect pattern |
| **AN885** | Microchip (Yedamale) | "Brushless DC (BLDC) Motor Fundamentals" — covers windmilling re-attach |
| **AN3208** | STMicroelectronics | "Sensorless field-oriented control of BLDC motors" |

### Six-step Hall commutation tables

The `Motor_step_tab_cw[6]`, `Motor_step_tab_ccw[6]`, and `CMP_PhaseTab[6]` lookup tables in `src/COMP_Sensoeless.c` encode the standard six-step trapezoidal commutation pattern. This is universal across all BLDC controllers:

- **TI SPRA588** — "Hall-effect sensor-based control of BLDC motors"
- **Microchip AN857** — section "Six-step Commutation"
- **NXP AN531** — "3-Phase BLDC Motor Control Algorithm"

The specific phase ordering matches the physical PWM-bridge wiring of the SNR8503M board; another board with different gate-driver routing would need a different table.

### DAC-driven over-current comparator threshold

Implemented in `periph_driver/source/snr8503x_dac.c` + `src/hardware_init.c`. The pattern (DAC sets a reference voltage that a comparator compares against the current-shunt amplifier output, tripping a hardware fault on over-current) appears in essentially every brushless motor driver IC and is documented in:

- **TI SLAA459** — "Implementing Cycle-by-Cycle Current Limit in a BLDC Controller"
- **ST AN4055** — "BLDC motor sensorless control by means of DC link current measurement"

### Three-phase PWM dead-time generation

The dead-time configuration in `MC_Parameter.h` is consumed by the MCPWM peripheral. The need for dead-time (to prevent shoot-through current on the half-bridge transitions) is universal — see:

- **IR AN-978** (now Infineon) — "HV Floating MOS-Gate Driver ICs"
- **ON Semi AND8154** — "Understanding Dead-Time in Motor Control"

### PI speed regulator

`src/PID.c` implements a textbook discrete-time PI controller with anti-windup. References:

- Åström & Hägglund, "PID Controllers: Theory, Design, and Tuning" (ISA Press, 1995)
- TI SPRA083 — "Implementing PID Controllers with the TMS320 Family"

### Bipropellant protocol

The MIT-licensed [bipropellant-protocol](https://github.com/bipropellant/bipropellant-protocol) project, in `third_party/bipropellant-protocol/`. The binary framing (`SOM1` `SOM2` `CI` `LEN` `CMD` `CODE` `PARAMS...` `CSUM`) and the ASCII variant are documented in that upstream project's README. We use it verbatim with no modifications.

---

## Acknowledgements

### Projects and libraries used

| Project | Role | License | Where |
|---|---|---|---|
| **arm-none-eabi-gcc** | C compiler, linker, objcopy | GPL-3.0 (with runtime library exception) | Toolchain |
| **newlib-nano** | Minimal C runtime for embedded | BSD-derived | `--specs=nano.specs --specs=nosys.specs` |
| **bipropellant-protocol** | Binary + ASCII serial protocol layer | MIT | `third_party/bipropellant-protocol/` |
| **LINKO platform_software** | Peripheral drivers (GPIO, ADC, DAC, MCPWM, HALL, UART, etc.) | BSD-3-Clause | `periph_driver/` (re-branded by SNANER as SNR8503x) |
| **PlatformIO toolchain-gccarmnoneeabi** | Bundled GCC toolchain used during this project's development | Apache-2.0 (the bundle) | Auto-detected by `build.sh` if `ARM_TOOLCHAIN` is unset |
| **CMSIS-Core(M)** | ARM Cortex-M CMSIS headers | Apache-2.0 / BSD | `periph_driver/include/` (re-distributed by LINKO) |

### People and organisations

- **LINKO SEMICONDUCTOR** — for the LKS32MC03x silicon and the BSD-3-Clause peripheral driver release that made this port practical
- **SNANER SEMICONDUCTOR** — for re-branding LKS32MC03x as SNR8503M and publishing the reference application code
- **bipropellant project contributors** — for the well-documented MIT binary protocol used in `MODULE_PROTOCOL_BIPROPELLANT_*`
- **PlatformIO** — for bundling a working `arm-none-eabi-gcc` toolchain
- **Texas Instruments, STMicroelectronics, NXP, Microchip, Atmel** — for the public application-note literature that documents every sensorless and Hall-based BLDC algorithm used here, making the closed `.lib` files unnecessary (see [Public-domain algorithm references](#public-domain-algorithm-references) above)

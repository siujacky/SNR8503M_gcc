# SNR8503M GCC Firmware — Migration Plan (Self-Loop Edition)

**Generated:** 2026-05-12
**Mode:** Night-build / autonomous self-loop execution
**Goal:** Build the SNR8503M Hall-BLDC motor firmware from the command line with GNU Arm GCC, integrate the bipropellant serial protocol, no Keil µVision IDE required.

---

## Locked decisions

| # | Decision | Value |
|---|---|---|
| 1 | License | **Skipped** (self project, not for distribution) |
| 2 | Build system | **GNU Make** |
| 3 | Closed-lib strategy | **Try linking ARMCC libs with GCC first**; stub if it fails |
| 4 | Optional `ascii_protocol.c` | **Include** (small Flash cost, terminal debug useful) |
| 5 | Hall-lib clean-room rewrite | **Deferred** (separate phase, not blocking) |
| 6 | Folder name | **`SNR8503M_gcc`** (renamed from `_gpl`) |

---

## 🌙 Self-loop execution protocol

This plan runs autonomously across multiple loop iterations (`/loop` or scheduled wake-ups). State persists in `STATUS.md`.

### Each invocation, exactly:

```
1. Read STATUS.md          → { current_phase, current_step, last_result }
2. If status = COMPLETE    → exit, nothing to do
3. If status = BLOCKED     → exit, await user (do NOT retry blocked steps)
4. Read PLAN.md            → find action + verify + fallback for current_step
5. Execute the action
6. Run the verification command
7. If verify passes:
     - Append to BUILD_LOG.md (timestamp, step, summary)
     - Update STATUS.md: mark step DONE, advance to next step
     - Exit → next loop iteration picks up the next step
8. If verify fails:
     - Run the fallback (one attempt)
     - If fallback verify passes: log + advance (same as 7)
     - If fallback also fails:
       - Append full error to BUILD_LOG.md
       - Update STATUS.md: mark step BLOCKED + reason
       - Exit → loop will not retry until user intervenes
9. If all phases DONE: STATUS.md → COMPLETE, exit
```

### Block escalation (require user, stop looping)

- Compilation error you've already attempted to fix once
- Linker error with no clear cause
- Hex file size grossly outside expected range (>2x or <0.5x ARMCC build's 11 KB)
- Any Phase 4 step (needs hardware)

### Preserve always

- Existing Keil project at `C:\RC\SNR8503M-Hall-main\SNR8503M_BLDC_Hall_LIB_V24\` — **read-only, never modify**. Reference build.
- ARMCC build artifacts at `...\Obj\` — used to compare hex size and verify behavior.

---

## Reusable assets

### `bipropellant-protocol` (MIT, copy into `third_party/`)
- `protocol.c`, `machine_protocol.c`, `ascii_protocol.c`, `cobsr.c`
- `protocol.h`, `cobsr.h`, `protocol_private.h`
- ~93 KB source → expect ~6-10 KB Flash compiled

Callbacks needed:
- `send_serial_data(buf, len)` → wraps `UART0_SendArray()`
- `send_serial_data_wait(buf, len)` → same (already blocking)
- `protocol_GetTick()` → ms counter from `nSys_TimerPWM/16`
- `protocol_Delay(ms)` → wraps `SoftDelay()`
- `protocol_SystemReset()` → `NVIC_SystemReset()` (CMSIS)

### `bipropellant-hoverboard-firmware` (GPL-3.0, reference patterns only — DO NOT copy verbatim)
- `Makefile` → template for our Cortex-M0 Makefile
- `STM32F103RCTx_FLASH.ld` → reference for linker script structure
- `startup_stm32f103xe.s` → reference for armasm V5 → GNU as
- `src/protocolfunctions.c` → patterns for `setup_protocol()` / `setParamVariable()` / `setParamHandler()`

### BSD LKS source (`C:\RC\platform_software-main\Release_Keil_Code_V2.1.1\V02_1_1_release_singleMotor\...\lks32mc03x_periph_driver\`)
- Use as fallback substitutes if any `snr8503x_*.c` files cause GCC compatibility issues

### Closed libs (link directly)
- `C:\RC\SNR8503M-Hall-main\SNR8503M_BLDC_Hall_LIB_V24\Kernal_Source\SNR_BLDC_HALL_V1p0.lib` (3 functions, ~414 bytes)
- `C:\RC\SNR8503M-Hall-main\SNR8503M_BLDC_Hall_LIB_V24\SNR8503x_Periph_Driver\Source\snr8503x_nvr.lib` (1 function used, ~90 bytes)

---

## Phase 1 — GCC build infrastructure

### Step 1.1 — Tool sanity check
- **Action:** `arm-none-eabi-gcc --version && arm-none-eabi-ld --version && arm-none-eabi-objcopy --version`
- **Verify:** All exit 0, version strings printed.
- **Pass:** All present.
- **Fallback:** Add `C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.3 rel1\bin` to PATH.
- **Block:** Still not found after PATH fix.

### Step 1.2 — Directory skeleton
- **Action:** Create: `src/ include/ periph_driver/source/ periph_driver/include/ startup/ third_party/bipropellant-protocol/ vendor_libs/ build/ docs/`
- **Verify:** All eight exist.
- **Pass:** `test -d` succeeds for each.

### Step 1.3 — Copy source files (reference, don't fork)
- **Action:** Copy from Keil project into new layout (Windows: use `cp`, not symlinks):
  - `src/` ← `SNR8503M_BLDC_Hall_LIB_V24/Kernal_Source/*.c` (except the `.lib`) + `AppFunction/*.c`
  - `include/` ← `SNR8503M_BLDC_Hall_LIB_V24/Include/*.h`
  - `periph_driver/source/` ← `SNR8503M_BLDC_Hall_LIB_V24/SNR8503x_Periph_Driver/Source/*.c`
  - `periph_driver/include/` ← `SNR8503M_BLDC_Hall_LIB_V24/SNR8503x_Periph_Driver/include/*.h`
  - `startup/startup_snr8503x.s` ← `SNR8503M_BLDC_Hall_LIB_V24/SNR8503x_Periph_Driver/include/startup_snr8503x.s` (original armasm — kept for reference; converted in 1.4)
  - `vendor_libs/` ← both `.lib` files
- **Verify:** `find src -name '*.c' | wc -l` ≥ 12. `find include -name '*.h' | wc -l` ≥ 15. `ls vendor_libs/*.lib | wc -l` = 2.
- **Pass:** All counts met.

### Step 1.4 — Convert startup to GNU as syntax
- **Action:** Write `startup/startup_snr8503x_gnu.S` (capital `.S`) based on the armasm V5 source:
  - Top: `.syntax unified`, `.cpu cortex-m0`, `.thumb`
  - `AREA |.text|, CODE, READONLY` → `.section .text`
  - `EXPORT name` → `.global name` + `.type name, %function`
  - `IMPORT name` → no directive; `extern` resolved at link
  - `DCD value` → `.word value`
  - `[WEAK]` after EXPORT → `.weak name`
  - `B .` → `b .`
  - `THUMB` → omit (use `.thumb_func` before function labels)
  - `END` → omit
  - Stack/heap symbols: define `__StackTop` and `__StackLimit` via linker script (1.5)
- **Verify:** `arm-none-eabi-as -mcpu=cortex-m0 -mthumb startup/startup_snr8503x_gnu.S -o build/startup.o` → exit 0.
- **Pass:** Object created, no errors.
- **Fallback:** Use `arm-none-eabi-gcc -E -P` to preprocess for debug; check for missing `.cfi_*` annotations.
- **Block:** Unresolved syntax errors after fallback.

### Step 1.5 — Write `linker.ld`
- **Action:** Create with SNR8503M memory map:
  ```
  MEMORY {
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 256K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
  }
  _stack_top = ORIGIN(RAM) + LENGTH(RAM);
  SECTIONS {
    .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
    .text :       { *(.text*) *(.rodata*) _etext = .; } > FLASH
    .data :       { _sdata = .; *(.data*) _edata = .; } > RAM AT > FLASH
    .bss  :       { _sbss = .; *(.bss*) *(COMMON) _ebss = .; } > RAM
    .ram3functions : { *(ram3functions) } > RAM         /* matches RUN_IN_RAM_FUNC attr */
  }
  ENTRY(Reset_Handler)
  ```
- **Verify:** File exists with `ENTRY(Reset_Handler)`.
- **Pass:** File present.

### Step 1.6 — Write `Makefile`
- **Action:** Top-level Makefile:
  ```
  TOOLCHAIN = arm-none-eabi-
  CC = $(TOOLCHAIN)gcc
  AS = $(TOOLCHAIN)gcc
  LD = $(TOOLCHAIN)gcc
  OBJCOPY = $(TOOLCHAIN)objcopy
  SIZE = $(TOOLCHAIN)size

  CPUFLAGS = -mcpu=cortex-m0 -mthumb
  CFLAGS = $(CPUFLAGS) -Os -ffunction-sections -fdata-sections -Wall -std=c99 \
           -Iinclude -Iperiph_driver/include -Ithird_party/bipropellant-protocol
  ASFLAGS = $(CPUFLAGS) -x assembler-with-cpp
  LDFLAGS = $(CPUFLAGS) -T linker.ld -nostartfiles \
            -Wl,--gc-sections -Wl,-Map=build/snr8503m.map

  SRCS_C = $(wildcard src/*.c) $(wildcard periph_driver/source/*.c) \
           $(wildcard third_party/bipropellant-protocol/*.c)
  SRCS_S = startup/startup_snr8503x_gnu.S
  OBJS = $(SRCS_C:.c=.o) $(SRCS_S:.S=.o)
  VENDOR_LIBS = vendor_libs/SNR_BLDC_HALL_V1p0.lib vendor_libs/snr8503x_nvr.lib

  all: build/snr8503m.hex build/snr8503m.bin

  build/snr8503m.elf: $(OBJS) | build
      $(LD) $(LDFLAGS) -o $@ $(OBJS) $(VENDOR_LIBS)
      $(SIZE) $@

  build/snr8503m.hex: build/snr8503m.elf
      $(OBJCOPY) -O ihex $< $@
  build/snr8503m.bin: build/snr8503m.elf
      $(OBJCOPY) -O binary $< $@
  build:
      mkdir -p build

  clean:
      rm -f $(OBJS) build/snr8503m.*
  ```
- **Verify:** `make -n all` (dry run) prints commands without error.
- **Pass:** Dry-run succeeds.

### Step 1.7 — First build (ARMCC lib link test)
- **Action:** `make clean && make all 2>&1 | tee build/first_build.log`
- **Verify:** `build/snr8503m.elf` and `.hex` exist. `arm-none-eabi-size build/snr8503m.elf` shows text 5,000-25,000 bytes (reference: 10,620).
- **Pass:** ELF+HEX present, size sane, no undefined symbols in log.
- **Fallback 1.7a:** If undefined `__rt_*`/`__aeabi_*`: add `-lgcc --specs=nano.specs -lc -lm` to LDFLAGS, retry.
- **Fallback 1.7b:** If only the two `.lib` files cause unresolved references after 1.7a: set STATUS → next step = Phase 2.1 (stubs).

## Phase 2 — Closed-lib stubs (conditional, only if 1.7b triggers)

### Step 2.1 — Stub file
- **Action:** Write `src/snr_blackbox_stubs.c`:
  ```c
  #include <stdint.h>
  #include "SNR_BLDC_HALL_V1p0.h"

  /* NVR: return 0; DAC/Hall slightly uncalibrated but functional */
  uint32_t Read_Trim(uint32_t addr) { (void)addr; return 0; }

  /* ICP state expected by interrupt.c (8 vars, 28 bytes) */
  volatile uint8_t  ICP_MODE = 0, ICP_SetFLAG = 0;
  volatile uint16_t ICP_ResumeCNT = 0, ICP_SetCNT = 0;
  volatile uint16_t ICP_LoadCNT = 0, ICP_LOWCNT = 0;
  volatile uint16_t ICP_LOWCNT_BAK = 0, ICP_HIGCNT = 0;

  /* Hall lib: placeholders. Motor won't commutate correctly. */
  void BLDC_init(void) { /* TODO clean-room rewrite */ }
  uint8_t HALL_Update(HALL_NEWVALUE_TypeDef *p) { return p ? p->u8HALL_NEW_Value : 0; }
  void ICP_Loader(void) { /* TODO clean-room rewrite */ }
  ```
- **Verify:** Compiles standalone.

### Step 2.2 — Remove vendor libs from Makefile
- **Action:** Remove `$(VENDOR_LIBS)` from the ELF link line.
- **Verify:** `make clean && make all` succeeds.
- **Pass:** Build succeeds (Note: motor non-functional; record `motor_runs=false` in STATUS).

## Phase 3 — Bipropellant integration

### Step 3.1 — Vendor protocol library
- **Action:** Copy `bipropellant-protocol` source from a local clone (or fetch via `gh repo clone bipropellant/bipropellant-protocol /tmp/bp` then copy) into `third_party/bipropellant-protocol/`:
  - `protocol.c`, `protocol.h`, `protocol_private.h`
  - `machine_protocol.c`, `ascii_protocol.c`
  - `cobsr.c`, `cobsr.h`
  - Upstream `LICENSE` (MIT)
- **Verify:** `ls third_party/bipropellant-protocol/*.c | wc -l` ≥ 4.
- **Pass:** Files present.

### Step 3.2 — Millisecond tick counter
- **Action:** Create `src/timebase.c`:
  ```c
  #include <stdint.h>
  volatile uint32_t g_ms_tick = 0;
  static uint16_t pwm_div = 0;
  /* Called from ADC_IRQHandler; PWM at 16 kHz, divide by 16 for ms */
  void timebase_pwm_tick(void) { if (++pwm_div >= 16) { pwm_div = 0; g_ms_tick++; } }
  uint32_t get_ms_tick(void) { return g_ms_tick; }
  ```
  Edit `src/interrupt.c` `ADC_IRQHandler`: add `timebase_pwm_tick();` near top.
  Add `extern uint32_t get_ms_tick(void);` to a new header `include/timebase.h`.
- **Verify:** Build succeeds, no warnings.

### Step 3.3 — Protocol callbacks
- **Action:** Write `src/protocol_handlers.c`:
  - `PROTOCOL_STAT sUART0;`
  - Callback wrappers (5 functions)
  - `setup_uart_protocol()` — assigns callbacks, calls `protocol_init(&sUART0)`, registers params (3.4)
- **Verify:** Compiles, links.

### Step 3.4 — Register parameters
- **Action:** In `setup_uart_protocol()`, register the 10 initial parameters per the table in the original plan (codes 0x01-0x0A mapping to `Motor.*` and `User_sys.*` globals).
- **Verify:** Compiles; manual code review confirms 10 register calls.

### Step 3.5 — Wire into scheduler
- **Action:** Edit `src/main.c`:
  - Add `#include "protocol.h"`, `extern PROTOCOL_STAT sUART0;`
  - In `main()` after `Hardware_init()`: `setup_uart_protocol();` (gated on `UART0_FUNCTION == ENABLE_FUNCTION`)
  - In `Task_Scheduler` 1ms slot, replace `UartDealRX/TX` with:
    ```c
    while (rxd_comm0.cnt > 0) {
        uint8_t b = rxd_comm0.buffer[rxd_comm0.read_pt];
        rxd_comm0.read_pt = (rxd_comm0.read_pt + 1) % UART0_BUF_SIZE;
        __disable_irq(); rxd_comm0.cnt--; __enable_irq();
        protocol_byte(&sUART0, b);
    }
    ```
  - In 10ms slot: `protocol_tick(&sUART0);`
- **Verify:** Build succeeds, text size ≤ 25 KB.

### Step 3.6 — Enable UART
- **Action:** Edit `include/MC_Parameter.h:150`: `DISABLE_FUNCTION` → `ENABLE_FUNCTION`.
- **Verify:** Rebuild; `grep -E "UART0_init|protocol_byte" build/snr8503m.map` shows both.
- **Pass:** Symbols present.

## Phase 4 — Hardware verification (NOT autonomous)

These need a J-Link, board, and serial adapter. The loop must mark these BLOCKED with `reason=needs_hardware` and exit.

### 4.1 Flash via `make flash` (target uses JLinkExe).
### 4.2 Read parameter 0x05 (bus voltage) via bipropellant Python tools.
### 4.3 Write 0x01=1000 (target speed) → motor should spin (only if Phase 1 ARMCC link succeeded).
### 4.4 Subscribe to 0x04 — verify periodic stream.
### 4.5 Diff `build/snr8503m.hex` against `SNR8503M_BLDC_Hall_LIB_V24/Obj/SNR_MC_Project.hex` (excluding protocol region).

---

## Time estimate

| Scenario | Hours |
|---|---|
| Phase 1 ARMCC link succeeds + Phase 3 + Phase 4 | **8-12** |
| Phase 2 stubs needed (motor non-functional) + Phase 3 | **6-10** |
| Full clean-room Hall-lib rewrite added | **+10-15** |

---

## File layout target (post-Phase 1)

```
SNR8503M_gcc/
├── PLAN.md                       # this file
├── STATUS.md                     # loop state (current phase/step + last result)
├── BUILD_LOG.md                  # running log of all loop iterations
├── README.md                     # written end of Phase 1
├── Makefile
├── linker.ld
├── docs/
├── build/                        # generated, gitignored
├── src/                          # application + new protocol_handlers.c + timebase.c
├── include/
├── periph_driver/source/
├── periph_driver/include/
├── startup/
│   ├── startup_snr8503x.s        # original armasm (reference)
│   └── startup_snr8503x_gnu.S    # converted GNU as
├── third_party/bipropellant-protocol/
└── vendor_libs/                  # SNR_BLDC_HALL_V1p0.lib, snr8503x_nvr.lib
```

---

## Pause / resume

When invoked via `/loop` or wake-up:
1. Read `STATUS.md` → current step
2. Read last entry of `BUILD_LOG.md` → most recent activity
3. Execute current step per PLAN.md (above)
4. Update STATUS.md + append BUILD_LOG.md
5. Exit; await next iteration

If a step needs >5 min wall-clock, split into sub-steps and persist sub-progress in STATUS.md. **Never assume the next iteration remembers anything** — everything important lives in the files.

---

# Phase 5+ Addendum — Hoverboard wire-compat (Option B) and Option C path

**Added:** 2026-05-12 (after Phase 1+3 build succeeded). Phase 4 hardware verification is **deferred**; we'll verify Phase 5 firmware on bench instead.

Full design + rationale is in **`OptionC_PLAN.md`**. This addendum holds only the executable step list for the self-loop.

## Design principles (why B can upgrade cleanly to C)

1. **Don't squat on hoverboard codes** — SNR custom params live at `0x40-0x4F`; hoverboard's `0x01-0x07, 0x0D, 0x80+` stay free for Phase 6/7/8 to claim.
2. **Full struct shapes from day one** — when we implement `0x08 electrical_measurements`, we fill the whole hoverboard struct with `motors[1]=zeros`. Phase 6/7 just stop zeroing.
3. **Pure additive across phases** — new code goes in new files. Existing files (Phase 1+3) only get touched once during Phase 5.1 (move SNR params out of `protocol_handlers.c` into `protocol_params_snr.c`).
4. **Infrastructure now, payoff later** — Hall-tick accumulator added in 5.2 is consumed by 6.1/6.2/6.4 without further changes.

---

## Phase 5 — Option B (Wire-compat layer)

**Goal:** Hoverboard-aware bipropellant tools work for basics (`read 0x08`, `write 0x09`); SNR-specific stuff lives at safe non-conflicting codes.

### Step 5.1 — Move SNR custom params 0x01-0x0A → 0x40-0x49
- **Action:** Create `src/protocol_params_snr.c` with a `void register_snr_params(PROTOCOL_STAT *s)` function. Move the 10 `setParamVariable` calls from `protocol_handlers.c::setup_uart_protocol()` into it, changing codes 0x01→0x40, 0x02→0x41, … 0x0A→0x49. Replace the inline calls in `setup_uart_protocol()` with a single call to `register_snr_params(&sUART0)`.
- **Verify:** Build clean. Map shows symbols at new codes.
- **Pass:** Build OK; `grep -E "0x4[0-9]" build/snr8503m.map` ≥ 10 hits.

### Step 5.2 — Hall-tick accumulator
- **Action:** Create `src/hall_accumulator.c` + `include/hall_accumulator.h`:
  ```c
  extern volatile int32_t  g_hall_ticks_abs;     /* +1 per Hall edge if CW, −1 if CCW */
  extern volatile uint32_t g_hall_ticks_period_us;
  void hall_accum_tick(uint8_t direction);
  ```
  Wire `hall_accum_tick(Motor.BLDC.u8Direction)` into `HALL_IRQHandler` in `src/interrupt.c` after the Hall value read.
- **Verify:** Build clean. Static lint: `grep -n hall_accum_tick src/interrupt.c` returns ≥ 1.

### Step 5.3 — Implement 0x08 electrical_measurements
- **Action:** Create `src/protocol_params_standard.c`. Declare a module-local `static PROTOCOL_ELECTRICAL_PARAMS g_electrical;`. Implement `void register_standard_params(PROTOCOL_STAT *s)` that uses `setParamHandler` for code 0x08:
  - On READ: populate `g_electrical` from current globals — `batteryVoltage = nBUS_Vol_ADC * (3.6f / 32752.0f) * VOLTAGE_SHUNT_RATIO`; `motors[0].dcAmps = u32Ibus_Final / CURRENT_ADC_PER_A`; `board_temp_raw = nMOS_NTC_ADC`; `motors[1] = {0}`.
  - On WRITE: ignore (read-only).
  - Then return the struct.
- **Verify:** Build clean. Size still under 28 KB text.

### Step 5.4 — Implement 0x09 protocol_enable
- **Action:** In `register_standard_params`, add `setParamVariable(s, 0x09, UI_CHAR, &Motor.BLDC.u8FlagEnMotorRun, 1);`
- **Verify:** Build clean.

### Step 5.5 — Stubs for 0x0A, 0x0B, 0x21
- **Action:** In `register_standard_params`:
  - 0x0A → `setParamVariable` to module-local `static volatile uint8_t g_disable_poweroff;`
  - 0x0B → `setParamVariable` to module-local `static volatile uint8_t g_debug_out;`
  - 0x21 → `setParamHandler` whose write callback parses `PROTOCOL_BUZZER_DATA` and calls `LED_ON` then `LED_OFF` after `buzzerLen` ms (use the existing ms tick to time the toggle in a non-blocking way; for B simplicity, just toggle LED_ONOFF once).
- **Verify:** Build clean.

### Step 5.6 — Add WHEEL_RADIUS_MM + HALL_TICKS_PER_REV (unused in B)
- **Action:** Append to `include/MC_Parameter.h`:
  ```c
  #define WHEEL_RADIUS_MM     (50)    /* TODO: set per actual hardware; placeholder for Phase 6+ */
  #define HALL_TICKS_PER_REV  (6 * MOTOR_POLES)
  ```
- **Verify:** Build clean. Unused constants warning is acceptable.

### Step 5.7 — Build verification
- **Action:** `./build.sh rebuild`
- **Verify:** `arm-none-eabi-size build/snr8503m.elf` text ≤ 28 KB; HEX produced; map contains both `register_standard_params` and `register_snr_params`.
- **Pass:** All conditions met.
- **Block:** Size > 32 KB cap, or linker errors.

### Step 5.8 — Update protocol codes doc
- **Action:** Create `docs/protocol_codes.md` with a table of all registered codes (5.3-5.5 standard + 5.1 SNR), each with: byte format, R/W, units, example bipropellant command line. (Skip the "/loop should generate sample Python" embellishment — just the reference table.)
- **Verify:** File exists with ≥ 12 code entries.

**Phase 5 total: ~2-3 hours autonomous. Ends BLOCKED needs_hardware (same as Phase 4 was), but with the wire-compat surface ready.**

---

## Phase 6 — Option C tier 1: Position telemetry (autonomous-safe)

Pure additive. Consumes the Hall accumulator from 5.2.

### 6.1 — `src/protocol_params_position.c` (NEW)
- `void register_position_params(PROTOCOL_STAT *s)` registers handlers for:
  - **0x02 HallData** (`PROTOCOL_HALL_DATA_STRUCT`): `HallPosn = g_hall_ticks_abs`; `HallSpeed = Motor.Control.u32MotorEleRPM_Final`; `HallPosnMultiplier = 1.0f / HALL_TICKS_PER_REV`; rest zero.
  - **0x04 Position** (`PROTOCOL_POSN`): `LeftAbsolute = g_hall_ticks_abs`; right + offsets zero.
  - **0x07 RawPosition** (4×i32): ticks + module-local offset.
  - **0x0C xytPosn** (`PROTOCOL_INTEGER_XYT_POSN`): `x = g_hall_ticks_abs * (2 * π * WHEEL_RADIUS_MM / HALL_TICKS_PER_REV)`; `y, degrees = 0` (single-track).
- Call `register_position_params(&sUART0)` from `setup_uart_protocol`.
- **Verify:** Build clean.

### 6.2 — Verification only (deferred until hardware)
- Hardware test: spin motor manually, verify 0x04 read returns non-zero ticks; verify 0x02 HallSpeed matches dashboard tachometer.

**Phase 6 total: 3-4 hours coding + hardware verification.**

---

## Phase 7 — Option C tier 2: Write commands + new control modes (NOT autonomous)

This phase introduces a new control mode (position-controlled commutation) and direct PWM override. Both need supervised hardware testing per step. **Self-loop must mark BLOCKED `reason=needs_supervised_test` before each substep.**

### 7.1 — `src/control_mode.c` + `include/control_mode.h` (NEW)
- Enum `ctrl_mode_t {MODE_VSP, MODE_SPEED_CMD, MODE_PWM_DIRECT, MODE_POSITION}` + a global `volatile ctrl_mode_t g_ctrl_mode`.
- `Task_Scheduler` 1 ms slot reads `g_ctrl_mode` and dispatches accordingly (existing VSP-pot path becomes the default).

### 7.2 — `src/protocol_params_motion.c` (NEW)
- **0x03 SpeedData** (`PROTOCOL_SPEED_DATA`): on WRITE convert `wanted_speed_mm_per_sec[0]` to RPM, set `Motor.USER.u16VSP_filt_value`, set `g_ctrl_mode = MODE_SPEED_CMD`. Telemetry fields populated on READ.
- **0x0D PWMData** + **0x0E PWMData.pwm**: on WRITE bypass state machine, set `g_ctrl_mode = MODE_PWM_DIRECT`, write `MCPWM_TH*` directly. Bounded by `speed_max_power`.
- **0x01 sensor_copy** (`PROTOCOL_SENSOR_FRAME`): synthesise `Angle` from VSP pot, header/duplicate bytes hardcoded.

### 7.3 — `src/position_control.c` + `include/position_control.h` (NEW)
- PI controller fed by `(target_ticks - g_hall_ticks_abs)` error.
- **0x05 PositionIncr** (`PROTOCOL_POSN_INCR`): on WRITE, set `target_ticks += Left * HALL_TICKS_PER_REV / (2π * WHEEL_RADIUS_MM)`, set `g_ctrl_mode = MODE_POSITION`.
- **0x06 PosnData** (`PROTOCOL_POSN_DATA`): config + telemetry.

### 7.4 — Edit `src/M1_StateMachine.c` — add `M1_RunState_PositionControl`
- **One existing-file edit** in the whole plan. Adds a new sub-state that doesn't transition out via Hall edges (transitions on position error within tolerance).

**Phase 7 total: 10-15 hours. Each step verified on hardware before proceeding to next.**

---

## Phase 8 — Option C tier 3: Flash-backed parameters

Makes PID gains and limits runtime-tunable.

### 8.1 — Flash sector reservation
- **Action:** Edit `linker.ld` to add `__flash_config_start = 0x0003F000;` and reserve the last 4 KB sector. Mark it `NOLOAD`.

### 8.2 — `src/flash_config.c` + `include/flash_config.h` (NEW)
- `FlashContent_t` struct matching hoverboard layout (magic + PID gains + limits + ADC settings + HoverboardEnable).
- `flash_config_load()` validates magic; if invalid → write defaults; copies to a RAM mirror `g_flash_cfg`.
- `flash_config_save()` writes `g_flash_cfg` back to Flash. Uses `snr8503x_Flash.c` erase + program. Throttle: at most one save per 60 s.

### 8.3 — `src/protocol_params_flash.c` (NEW)
- Register 0x80-0xA0 as `setParamVariable` over `g_flash_cfg` fields. Writes update the RAM mirror; a periodic check in `Task_Scheduler` 100 ms slot commits to Flash if dirty + grace period elapsed.

### 8.4 — Refactor PID gains from `#define` to runtime variables
- **The painful bit.** Replace every reference to `SSum_Kp`/`SSum_Ki`/`SSum_Kc`/`MAX_BUS_CURRENT_SETTINT`/`VSP_DUTY_ACC_LOAD` etc. with `g_flash_cfg.SpeedKpx100`/...
- Affected files: `src/PID.c`, `src/SpeedRamp.c`, `src/Protect.c`. ~30-40 call sites.
- Conversion: hoverboard codes use `*100` scaling; our codebase uses `Q15` (`<<15`). Either convert on write/read or keep them in `*100` and adapt the loops.

### 8.5 — Hardware verification
- Power cycle test: write 0x85 = 0.05 * 100 = 5, save, power cycle, verify 0x85 reads back 5 and Speed PI Kp behavior changed.

**Phase 8 total: 6-10 hours.**

---

## Summary table

| Phase | Goal | Hours | Autonomy |
|---|---|---|---|
| 4 (existing) | Hardware verification of current firmware | 1-2 (manual) | NOT autonomous (deferred) |
| 5 | Option B — wire-compat layer | 2-3 | ✅ Autonomous |
| 6 | Option C tier 1 — position telemetry | 3-4 | ✅ Autonomous (code); manual verify |
| 7 | Option C tier 2 — write cmds + new control modes | 10-15 | ❌ Supervised step-by-step |
| 8 | Option C tier 3 — flash-backed config | 6-10 | ❌ Supervised (Flash erase risk) |
| **B-only end state** | Hoverboard tools work for read/enable | **2-3** | |
| **Full C end state** | Drop-in single-motor hoverboard emulation | **21-32** | |

## Self-loop hand-off after Phase 5

When Phase 5 reaches 5.7+5.8 successfully, set:
- `status: BLOCKED`
- `blocked_reason: needs_hardware`
- `next_step: 5.9_hardware_test` (manual)

User flashes, runs a bipropellant Python read of 0x08 and 0x09 to verify wire-compat, then unblocks by setting `current_step: 6.1` and `status: READY`.


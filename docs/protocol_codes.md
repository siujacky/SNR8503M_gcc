# SNR8503M Bipropellant Protocol — Code Reference

**Generated:** 2026-05-12 (Phase 5.8 of GCC migration)
**Wire:** UART0 @ 9600 baud, 8N1, P1.6 (TX) / P1.7 (RX)
**Status:** Option B (wire-compat) — Phase 5 complete. Phase 6+ adds more codes.

## Timing budget (FIX2-013)

Protocol RX bytes are drained and TX responses generated from the **10 ms scheduler slot**, not the 1 ms slot. Implications:

- **Worst-case round-trip latency:** ~11 ms (1 ms scheduler granularity + up to 10 ms slot wait + ~1 ms processing)
- **Throughput:** Up to ~26 bytes per drain × 100 drains/sec = 2600 bytes/sec, well above 9600-baud line rate (1200 bytes/sec)
- **Host implication:** Don't expect <10 ms responses. Tight-latency hosts should use subscriptions (server pushes at fixed period) rather than synchronous request/response cycles

The 10 ms slot was chosen to reduce 1 ms scheduler jitter; ring buffer is 256 bytes (absorbs >200 ms of full-rate input).

## FlashContent_t schema version

Currently **version 2** (post-FIX-013, includes CRC32). Upgrading firmware from version 1 invalidates persisted tunings (defaults restored). Bump the version explicitly when changing the struct layout.

---

This is the reference for anyone writing a bipropellant host that talks to this firmware. See `OptionC_PLAN.md` for the full code-allocation map across all phases.

---

## Quick start (any bipropellant tool)

```python
# Generic Python host (example):
import bipropellant
mc = bipropellant.Protocol(port='/dev/ttyUSB0', baud=9600)
mc.write(0x09, b'\x01')           # enable motor
elec = mc.read(0x08)              # read electrical_measurements
print(elec.batteryVoltage, elec.motors[0].dcAmps)
mc.write(0x09, b'\x00')           # stop motor
```

---

## Implemented codes (Phase 5 — Option B)

### Standard hoverboard-compatible

| Code | Name | Direction | Struct/Type | Bytes | Notes |
|---|---|---|---|---|---|
| **0x08** | electrical_measurements | R | `PROTOCOL_ELECTRICAL_PARAMS` | ~88 | `motors[0]` populated; `motors[1]` always zeros (single-motor hardware). `batteryVoltage` is in volts (float), `motors[0].dcAmps` is in amps (float). |
| **0x09** | protocol_enable | RW | `u8` | 1 | 0=stop, 1=run. Same semantic as hoverboard. |
| **0x0A** | disablepoweroff | RW | `u8` | 1 | Accepted but ignored (no power-off relay on board). |
| **0x0B** | debug_out | RW | `u8` | 1 | Accepted but ignored (we use SEGGER RTT for debug). |
| **0x21** | BuzzerData | RW | `PROTOCOL_BUZZER_DATA` | 4 | Write toggles LED once (no buzzer hardware). `buzzerLen` ignored for now. |

### SNR8503M-specific (custom, non-conflicting range)

| Code | Name | Direction | Type | Bytes | Backing global |
|---|---|---|---|---|---|
| **0x40** | target_speed | RW | `u16` | 2 | `Motor.USER.u16VSP_filt_value` (overrides ADC pot when written) |
| **0x41** | direction | RW | `u8` | 1 | `Motor.BLDC.u8Direction` — CW=1, CCW=0 |
| **0x42** | run_enable_alt | RW | `u8` | 1 | Same backing as 0x09; duplicate for SNR-tool convenience |
| **0x43** | speed_rpm | R | `u32` | 4 | `Motor.Control.u32MotorEleRPM_Final` — filtered electrical RPM |
| **0x44** | bus_voltage_adc | R | `s16` | 2 | Raw ADC counts; multiply by `3.6/32752 * VOLTAGE_SHUNT_RATIO` for volts |
| **0x45** | bus_current | R | `u32` | 4 | Filtered ADC counts; divide by `CURRENT_ADC_PER_A` (≈48.5) for amps |
| **0x46** | fault_flags | RW | `u16` | 2 | Bitfield (see below). Write 0 to clear all. |
| **0x47** | mos_temp_adc | R | `s16` | 2 | Raw NTC ADC counts; lookup via NTC β-curve for °C |
| **0x48** | main_state | R | `u8` | 1 | 0=Fault, 1=Init, 2=Stop, 3=Run |
| **0x49** | run_substate | R | `u8` | 1 | 0=Calib, 1=Ready, 2=Freerun_Det, 3=Align, 4=Startup, 5=Spin, 6=Brake |

### Fault flag bits (0x46)

```
bit 0  SHORT_ERROR        bus short circuit
bit 1  LOW_VOL_ERROR      undervoltage
bit 2  HIG_VOL_ERROR      overvoltage
bit 3  BLOCK_ERROR        locked rotor
bit 4  DC_OFFSET_ERROR    ADC bias calibration failure
bit 5  MOS_OVER_ERROR     MOSFET overtemperature
bit 6  MOS_LOW_ERROR      MOSFET undertemperature
bit 7  BAT_OVER_ERROR     battery overtemperature
bit 8  BAT_LOW_ERROR      battery undertemperature
bit 9  OVER_LOAD_ERROR    overload (PowLim active)
bit 10 PHASE_DROP_ERROR   missing motor phase
bit 11 MOSFET_ERROR       MOS self-test failed at boot
```

---

## Future codes (NOT YET implemented)

Reserved for Phase 6-8. Hosts attempting to use these will receive a NACK or `?` unknown-command response.

### Phase 6 (autonomous-code, hardware-verify) — position telemetry

| Code | Name | Phase |
|---|---|---|
| 0x02 | HallData | 6.1 |
| 0x04 | Position | 6.1 |
| 0x07 | RawPosition | 6.1 |
| 0x0C | xytPosn (dead-reckoned) | 6.1 |

### Phase 7 (supervised) — write commands + new control modes

| Code | Name | Phase |
|---|---|---|
| 0x01 | sensor_copy (synthesised) | 7.2 |
| 0x03 | SpeedData | 7.2 |
| 0x05 | PositionIncr | 7.3 |
| 0x06 | PosnData | 7.3 |
| 0x0D | PWMData | 7.2 |
| 0x0E | PWMData.pwm | 7.2 |

### Phase 8 (supervised, Flash) — runtime-tunable config

| Code | Name | Phase |
|---|---|---|
| 0x80 | FlashContent.magic | 8.3 |
| 0x81–0x89 | PID gains + limits | 8.3 |
| 0x90 | ADC settings | 8.3 |
| 0xA0 | HoverboardEnable | 8.3 |

---

## Bipropellant wire commands (always supported, all phases)

These are protocol-layer opcodes, not parameter-specific. They work for every registered code automatically:

| Opcode | Char | Purpose |
|---|---|---|
| 0x52 | `R` | READVAL — request value |
| 0x72 | `r` | READVALRESPONSE — server reply with value |
| 0x57 | `W` | WRITEVAL — write value, request ACK |
| 0x77 | `w` | WRITEVALRESPONSE — server ACK |
| 0x73 | `s` | SILENTREAD — read but don't transmit (triggers handler) |
| 0x41 | `A` | ACK |
| 0x4E | `N` | NACK |
| 0x42 | `B` | REBOOT — `protocol_SystemReset()` is wired to `NVIC_SystemReset` |
| 0x54 | `T` / `t` | TEST / TESTRESPONSE |
| 0x3F | `?` | UNKNOWN reply |

Subscription support (periodic auto-emit) — yes, up to 10 simultaneous subscriptions. Subscribe to any registered code with a period in ms.

---

## Conversion tables (host-side)

These constants are baked into the firmware via `MC_Parameter.h`. Hosts plotting telemetry need them:

```
ADC reference voltage:       3.6 V
ADC full-scale (16-bit):     32752 counts
Voltage divider ratio:       (33 + 1) / 1 = 34.0   (bus voltage divider)
Shunt resistance:            0.004 Ω
Op-amp gain:                 18.18
CURRENT_ADC_PER_A:           RSHUNT * GAIN * 32752 / 3.6 ≈ 0.661 counts/A ?? Re-derive — see MC_Parameter.h line 64
PWM frequency:               16,000 Hz
Motor poles:                 3
HALL_TICKS_PER_REV:          6 × poles = 18
Wheel radius (Phase 6+):     50 mm  (TODO: tune to actual hardware)
```

For absolute voltage (volts):  `V = adc * (3.6 / 32752) * 34.0`
For absolute current (amps):  `A = adc / CURRENT_ADC_PER_A`

---

## Caveats

1. **`0x08 batteryVoltage` is in volts (float, IEEE 754).** Host must parse 4 bytes as a float, not assume integer. Same for `dcAmps`, `dcAmpsAvg`.
2. **No FPU.** All float math is soft-emulated. Each electrical_measurements read takes ~50 µs of CPU. Don't subscribe at 1 ms intervals.
3. **0x21 BuzzerData currently only flashes LED once**; `buzzerFreq` and `buzzerPattern` are ignored. (Future phase could PWM the LED for tone simulation.)
4. **Phase 5 has no position/distance info.** Hall edges are counted (`g_hall_ticks_abs`) but not surfaced. Wait for Phase 6.1 (0x02, 0x04, 0x07, 0x0C).
5. **`0x09` and `0x42` write to the same byte.** Use whichever feels natural — host writing 0x09 and SNR script writing 0x42 see the same motor enable state.

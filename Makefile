# SNR8503M GCC firmware Makefile
# Target: Cortex-M0 (LKS32MC03x via SNR8503M re-brand)
# Phase 1.6 — first GCC build, no Keil

# -----------------------------------------------------------------------------
# Toolchain
# -----------------------------------------------------------------------------
TOOLCHAIN_BIN := /c/Users/Dell/.platformio/packages/toolchain-gccarmnoneeabi/bin
CC      := $(TOOLCHAIN_BIN)/arm-none-eabi-gcc
AS      := $(TOOLCHAIN_BIN)/arm-none-eabi-gcc
LD      := $(TOOLCHAIN_BIN)/arm-none-eabi-gcc
OBJCOPY := $(TOOLCHAIN_BIN)/arm-none-eabi-objcopy
SIZE    := $(TOOLCHAIN_BIN)/arm-none-eabi-size

# -----------------------------------------------------------------------------
# Build directories
# -----------------------------------------------------------------------------
BUILD_DIR := build
TARGET    := snr8503m

# -----------------------------------------------------------------------------
# CPU & flags
# -----------------------------------------------------------------------------
CPU       := -mcpu=cortex-m0 -mthumb
OPT       := -Os
WARN      := -Wall -Wno-unused-function -Wno-unused-variable -Wno-pointer-sign
DEFS      :=

INCLUDES  := -Iinclude \
             -Iperiph_driver/include \
             -Ithird_party/bipropellant-protocol

CFLAGS    := $(CPU) $(OPT) $(WARN) $(DEFS) $(INCLUDES) \
             -ffunction-sections -fdata-sections \
             -std=gnu99 -fno-common -fmessage-length=0 \
             -MMD -MP

ASFLAGS   := $(CPU) -c

LDSCRIPT  := linker.ld
LDFLAGS   := $(CPU) -T $(LDSCRIPT) -nostartfiles \
             --specs=nano.specs --specs=nosys.specs \
             -Wl,--gc-sections \
             -Wl,-Map=$(BUILD_DIR)/$(TARGET).map

# No vendor libs — all former closed-lib symbols are now either inlined into
# the project source files or have been removed entirely (see README).
VENDOR_LIBS :=

# -----------------------------------------------------------------------------
# Source files
# -----------------------------------------------------------------------------
SRCS_C    := $(wildcard src/*.c) \
             $(wildcard periph_driver/source/*.c) \
             $(wildcard third_party/bipropellant-protocol/*.c)
SRCS_S    := startup/startup_snr8503x_gnu.S

OBJS      := $(SRCS_C:%.c=$(BUILD_DIR)/%.o) \
             $(SRCS_S:%.S=$(BUILD_DIR)/%.o)
DEPS      := $(OBJS:.o=.d)

# -----------------------------------------------------------------------------
# Targets
# -----------------------------------------------------------------------------
.PHONY: all clean size flash help dirs

all: dirs $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin size

dirs:
	@mkdir -p $(BUILD_DIR)/src \
	          $(BUILD_DIR)/periph_driver/source \
	          $(BUILD_DIR)/third_party/bipropellant-protocol \
	          $(BUILD_DIR)/startup

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJS) $(LDSCRIPT)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(VENDOR_LIBS) -lm

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

size: $(BUILD_DIR)/$(TARGET).elf
	$(SIZE) $<

clean:
	rm -rf $(BUILD_DIR)

flash: $(BUILD_DIR)/$(TARGET).hex
	@echo "TODO: integrate JLinkExe or openocd here"
	@echo "  manual: JLinkExe -device Cortex-M0 -if SWD -speed 4000 -CommanderScript flash.jlink"

help:
	@echo "make all   - build $(TARGET).{elf,hex,bin}"
	@echo "make clean - remove build/"
	@echo "make size  - show section sizes"
	@echo "make flash - flash via J-Link (TODO)"

-include $(DEPS)

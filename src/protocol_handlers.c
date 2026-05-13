/*
 * protocol_handlers.c — bipropellant protocol integration for SNR8503M.
 *
 * Phase 3.3 (callbacks) + 3.4 (parameter registry) + 3.5 (scheduler hooks).
 * Compiled only when UART0_FUNCTION is enabled in MC_Parameter.h.
 */

#include "MC_Parameter.h"

#if MODULE_PROTOCOL_BIPROPELLANT_BIN

#include <stdint.h>
#include <stddef.h>
#include "basic.h"
#include "snr8503x_uart.h"
#include "hardware_config.h"
#include "UR_Ctrl.h"
#include "Global_Variable.h"
#include "motor_structure.h"
#include "M1_StateMachine.h"
#include "timebase.h"
#include "protocol.h"

/* Per-module forward declarations — only declared if the module is built */
#if MODULE_CODES_SNR
extern void register_snr_params(PROTOCOL_STAT *s);
#endif
#if MODULE_CODES_STANDARD
extern void register_standard_params(PROTOCOL_STAT *s);
#endif
#if MODULE_CODES_POSITION
extern void register_position_params(PROTOCOL_STAT *s);
#endif
#if MODULE_CODES_MOTION
extern void register_motion_params(PROTOCOL_STAT *s);
#endif
#if MODULE_CODES_POSCTRL
extern void register_motion_pos_params(PROTOCOL_STAT *s);
extern void position_control_init(void);
#endif
#if MODULE_FLASH_CONFIG
extern void register_flash_params(PROTOCOL_STAT *s);
extern void flash_config_load(void);
#endif

/* ------------------------------------------------------------------------- */
/*  Protocol state                                                             */
/* ------------------------------------------------------------------------- */
PROTOCOL_STAT sUART0;

/* ------------------------------------------------------------------------- */
/*  Callback implementations                                                   */
/* ------------------------------------------------------------------------- */
extern volatile u8 UART_Flag;   /* defined in UR_Ctrl.c — set 0 on TX-complete IRQ */

static int my_send_serial_data(unsigned char *data, int len)
{
    for (int i = 0; i < len; i++) {
        UART_Flag = 1;
        UART0->BUFF = data[i];
        while (UART_Flag);              /* wait for TX-complete IRQ */
    }
    return len;
}

/* Same impl as the non-wait variant — UART0_SendArray was already blocking. */
static int my_send_serial_data_wait(unsigned char *data, int len)
{
    return my_send_serial_data(data, len);
}

static void my_protocol_delay(uint32_t ms)
{
    /* SoftDelay calibration: 7500 ≈ 1 ms on Cortex-M0 @ 48 MHz (rough). */
    while (ms--) SoftDelay(7500);
}

static void my_protocol_reset(void)
{
    NVIC_SystemReset();
}

static uint32_t my_protocol_gettick(void)
{
    return get_ms_tick();
}

/* ------------------------------------------------------------------------- */
/*  setup_uart_protocol — call once after Hardware_init.                       */
/* ------------------------------------------------------------------------- */
void setup_uart_protocol(void)
{
    /* Global function pointers (declared extern in protocol.h) */
    protocol_GetTick     = my_protocol_gettick;
    protocol_Delay       = my_protocol_delay;
    protocol_SystemReset = my_protocol_reset;

    /* Per-state callbacks */
    sUART0.send_serial_data      = my_send_serial_data;
    sUART0.send_serial_data_wait = my_send_serial_data_wait;

    protocol_init(&sUART0);

    /* Per-module registration — only the enabled modules contribute codes. */
#if MODULE_CODES_SNR
    register_snr_params(&sUART0);            /* 0x40-0x49 */
#endif
#if MODULE_CODES_STANDARD
    register_standard_params(&sUART0);       /* 0x08, 0x09, 0x0A, 0x0B, 0x21 */
#endif
#if MODULE_CODES_POSITION
    register_position_params(&sUART0);       /* 0x02, 0x04, 0x07, 0x0C */
#endif
#if MODULE_CODES_MOTION
    register_motion_params(&sUART0);         /* 0x01, 0x03, 0x0D, 0x0E */
#endif
#if MODULE_CODES_POSCTRL
    register_motion_pos_params(&sUART0);     /* 0x05, 0x06 */
    position_control_init();
#endif
#if MODULE_FLASH_CONFIG
    /* flash_config_load() called earlier from main() before motor_mode_detect */
    register_flash_params(&sUART0);          /* 0x80-0xA0 + 0xB0-0xB2 */
#endif
}

/* ------------------------------------------------------------------------- */
/*  Drains the existing rxd_comm0 ring buffer into protocol_byte.              */
/*  Called from Task_Scheduler 1ms slot.                                       */
/* ------------------------------------------------------------------------- */
void protocol_drain_rx(void)
{
    /* FIX-003: read/advance/decrement must all be inside one critical section.
     * protocol_byte() is called OUTSIDE the section so handler latency doesn't
     * block UART RX. */
    while (rxd_comm0.cnt > 0) {
        unsigned char b;
        NVIC_DisableIRQ(UART_IRQn);
        b = rxd_comm0.buffer[rxd_comm0.read_pt];
        rxd_comm0.read_pt = (rxd_comm0.read_pt + 1) % UART0_BUF_SIZE;
        rxd_comm0.cnt--;
        NVIC_EnableIRQ(UART_IRQn);
        protocol_byte(&sUART0, b);
    }
}

/* Called from Task_Scheduler 10ms slot. */
void protocol_drive_tick(void)
{
    protocol_tick(&sUART0);
}

#endif /* MODULE_PROTOCOL_BIPROPELLANT_BIN */

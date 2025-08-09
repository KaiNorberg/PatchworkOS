#pragma once

#include "cpu/port.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief PS/2 Driver.
 * @defgroup kernel_drivers_ps2 PS/2 Driver
 * @ingroup kernel_drivers
 *
 * Patchwork attempts to implement a comprehensive PS/2 driver, even if certain details would be completely irelevent
 * on modern hardware. For example, most implementations assume that the first PS2 device is always a keyboard and the
 * second a mouse, however this is, as far as im aware, just a very commonly agreed convention and not actually
 * specified anywhere, even if all modern hardware appears to follow this convention, they *technically* dont have to.
 *
 * The reason for this decision is... fun, and because PS/2 appears to be frequently neglected in hobby projects, so i
 * wish to create a publically available and easy to understand "proper" implementation.
 *
 * Note that in the future once proper device detection is implemented, this will need to be redone.
 *
 * Some sources I used while making this:
 * https://wiki.osdev.org/I8042_PS/2_Controller
 * https://www-ug.eecg.toronto.edu/msl/nios_devices/datasheets/PS2%20Keyboard%20Protocol.htm
 * @{
 */

typedef enum
{
    PS2_PORT_DATA = 0x60,
    PS2_PORT_STATUS = 0x64,
    PS2_PORT_CMD = 0x64
} ps2_port_t;

typedef enum
{
    PS2_CMD_CFG_READ = 0x20,
    PS2_CMD_CFG_WRITE = 0x60,
    PS2_CMD_SECOND_DISABLE = 0xA7,
    PS2_CMD_SECOND_ENABLE = 0xA8,
    PS2_CMD_FIRST_DISABLE = 0xAD,
    PS2_CMD_FIRST_ENABLE = 0xAE,
    PS2_CMD_SECOND_TEST = 0xA9,
    PS2_CMD_SELF_TEST = 0xAA,
    PS2_CMD_FIRST_TEST = 0xAB,
    PS2_CMD_SECOND_WRITE = 0xD4,
    PS2_CMD_OUTPUT_READ = 0xD0,
    PS2_CMD_OUTPUT_WRITE = 0xD1,
    PS2_CMD_PULSE_RESET = 0xFE,
    PS2_CMD_PULSE_BASE = 0xF0
} ps2_cmd_t;

typedef enum
{
    PS2_STATUS_OUT_FULL = (1 << 0), //!< Output buffer status (0 = empty, 1 = full)
    PS2_STATUS_IN_FULL = (1 << 1),  //!< Input buffer status (0 = empty, 1 = full)
    PS2_STATUS_SYSTEM_FLAG = (1 << 2),
    PS2_STATUS_CMD_DATA = (1 << 3), //!< Command(1) or Data(0)
    PS2_STATUS_TIMEOUT_ERROR = (1 << 6),
    PS2_STATUS_PARITY_ERROR = (1 << 7)
} ps2_status_bits_t;

typedef enum
{
    PS2_CFG_FIRST_IRQ = (1 << 0),            //!< First PS/2 port interrupt enable
    PS2_CFG_SECOND_IRQ = (1 << 1),           //!< Second PS/2 port interrupt enable
    PS2_CFG_SYSTEM_FLAG = (1 << 2),          //!< System flag (POST passed)
    PS2_CFG_RESERVED_3 = (1 << 3),           //!< Should be zero
    PS2_CFG_FIRST_CLOCK_DISABLE = (1 << 4),  //!< First PS/2 port clock disable
    PS2_CFG_SECOND_CLOCK_DISABLE = (1 << 5), //!< Second PS/2 port clock disable
    PS2_CFG_FIRST_TRANSLATION = (1 << 6),    //!< First PS/2 port translation enable
    PS2_CFG_RESERVED_7 = (1 << 7)            //!< Should be zero
} ps2_config_bits_t;

typedef enum
{
    PS2_DEV_RESET = 0xFF,
    PS2_DEV_RESEND = 0xFE,
    PS2_DEV_SET_DEFAULTS = 0xF6,
    PS2_DEV_DISABLE_SCANNING = 0xF5, // This is also disable data reporting for the mouse
    PS2_DEV_ENABLE_SCANNING = 0xF4, // This is also enable data reporting for the mouse
    PS2_DEV_SET_TYPEMATIC = 0xF3,
    PS2_DEV_IDENTIFY = 0xF2,
    PS2_DEV_SET_SCANCODE_SET = 0xF0,
    PS2_DEV_ECHO = 0xEE,
    PS2_KBD_SET_LEDS = 0xED,
    PS2_MOUSE_SET_SAMPLE_RATE = 0xF3,
    PS2_MOUSE_SET_RESOLUTION = 0xE8,
    PS2_MOUSE_STATUS_REQUEST = 0xE9,
    PS2_MOUSE_SET_STREAM_MODE = 0xEA,
    PS2_MOUSE_READ_DATA = 0xEB,
    PS2_MOUSE_SET_REMOTE_MODE = 0xF0,
    PS2_MOUSE_SET_WRAP_MODE = 0xEE,
    PS2_MOUSE_RESET_WRAP_MODE = 0xEC,
} ps2_device_cmd_t;

typedef enum
{
    PS2_DEVICE_NONE = -1,  //!< No device
    PS2_DEVICE_FIRST = 0,  //!< First PS/2 port
    PS2_DEVICE_SECOND = 1, //!< Second PS/2 port
    PS2_DEVICE_COUNT = 2   //!< Total number of ports
} ps2_device_t;

typedef enum
{
    PS2_DEVICE_TYPE_NONE,
    PS2_DEVICE_TYPE_KEYBOARD,
    PS2_DEVICE_TYPE_MOUSE,
} ps2_device_type_t;

typedef struct
{
    ps2_device_t device;
    ps2_device_type_t type;
    const char* name;
    uint8_t id[2];
    uint8_t idLength;
    void* data;
} ps2_device_info_t;

typedef enum
{
    PS2_SELF_TEST_PASS = 0x55,
    PS2_SELF_TEST_FAIL = 0xFC,
} ps2_self_test_response_t;

typedef enum
{
    PS2_DEVICE_TEST_PASS = 0x00,
    PS2_DEVICE_TEST_CLOCK_STUCK_LOW = 0x01,
    PS2_DEVICE_TEST_CLOCK_STUCK_HIGH = 0x02,
    PS2_DEVICE_TEST_DATA_STUCK_LOW = 0x03,
    PS2_DEVICE_TEST_DATA_STUCK_HIGH = 0x04,
} ps2_device_test_response_t;

typedef enum
{
    PS2_DEVICE_RESET_PASS_1 = 0xFA,
    PS2_DEVICE_RESET_PASS_2 = 0xAA,
} ps2_device_reset_response_t;

typedef enum
{
    PS2_DEVICE_ACK = 0xFA,
    PS2_DEVICE_RESEND = 0xFE,
} ps2_device_ack_t;

#define PS2_READ(data) \
    ({ \
        uint64_t result = ps2_wait_until_set(PS2_STATUS_OUT_FULL); \
        if (result != ERR) \
        { \
            *(data) = port_inb(PS2_PORT_DATA); \
        } \
        result; \
    })

#define PS2_WRITE(data) \
    ({ \
        uint64_t result = ps2_wait_until_clear(PS2_STATUS_IN_FULL); \
        if (result != ERR) \
        { \
            port_outb(PS2_PORT_DATA, data); \
        } \
        result; \
    })

void ps2_init(void);

void ps2_drain(void);

uint64_t ps2_wait_until_set(ps2_status_bits_t status);

uint64_t ps2_wait_until_clear(ps2_status_bits_t status);

uint64_t ps2_cmd(ps2_cmd_t command);

uint64_t ps2_device_cmd(ps2_device_t device, ps2_device_cmd_t command);

/** @} */

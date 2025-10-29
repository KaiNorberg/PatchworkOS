#pragma once

#include <kernel/cpu/port.h>

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief IBM Personal Computer/2 ports.
 * @defgroup kernel_drivers_ps2 PS/2
 * @ingroup kernel_drivers
 *
 * Patchwork attempts to implement a comprehensive PS/2 driver, even if certain details would be completely irrelevant
 * on modern hardware. For example, most implementations assume that the first PS2 device is always a keyboard and the
 * second a mouse, however this is, as far as I'm aware, just a very commonly agreed convention and not actually
 * specified anywhere, even if all modern hardware appears to follow this convention, they *technically* don't have to.
 *
 * The reason for this is because PS/2 appears to be frequently neglected in hobby projects, so I
 * wish to create a publically available and easy to understand "proper" implementation. Even if its overkill and of
 * course becouse its fun.
 *
 * Note that in the future once proper device detection is implemented, this will need to be redone.
 *
 * @see https://wiki.osdev.org/I8042_PS/2_Controller
 * @see https://www-ug.eecg.toronto.edu/msl/nios_devices/datasheets/PS2%20Keyboard%20Protocol.htm
 * @{
 */

/**
 * @brief Wait timeout for PS/2 controller
 */
#define PS2_WAIT_TIMEOUT (CLOCKS_PER_SEC / 2)

/**
 * @brief Small delay for various operations
 */
#define PS2_SMALL_DELAY (CLOCKS_PER_SEC / 100)

/**
 * @brief Large delay for various operations
 */
#define PS2_LARGE_DELAY (CLOCKS_PER_SEC / 5)

/**
 * @brief Number of retries for commands
 */
#define PS2_COMMAND_RETRIES 3

/**
 * @brief PS/2 controller I/O ports
 */
typedef enum
{
    PS2_PORT_DATA = 0x60,
    PS2_PORT_STATUS = 0x64,
    PS2_PORT_CMD = 0x64
} ps2_port_t;

/**
 * @brief PS/2 controller commands
 */
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
    PS2_CMD_DUMP = 0xAC,
    PS2_CMD_SECOND_WRITE = 0xD4,
} ps2_cmd_t;

/**
 * @brief PS/2 controller status register bits
 */
typedef enum
{
    PS2_STATUS_OUT_FULL = (1 << 0), ///< Output buffer status (0 = empty, 1 = full)
    PS2_STATUS_IN_FULL = (1 << 1),  ///< Input buffer status (0 = empty, 1 = full)
    PS2_STATUS_SYSTEM_FLAG = (1 << 2),
    PS2_STATUS_CMD_DATA = (1 << 3), ///< Command(1) or Data(0)
    PS2_STATUS_TIMEOUT_ERROR = (1 << 6),
    PS2_STATUS_PARITY_ERROR = (1 << 7)
} ps2_status_bits_t;

/**
 * @brief PS/2 controller configuration bits
 */
typedef enum
{
    PS2_CFG_FIRST_IRQ = (1 << 0),            ///< First PS/2 port interrupt enable
    PS2_CFG_SECOND_IRQ = (1 << 1),           ///< Second PS/2 port interrupt enable
    PS2_CFG_SYSTEM_FLAG = (1 << 2),          ///< System flag (POST passed)
    PS2_CFG_RESERVED_3 = (1 << 3),           ///< Should be zero
    PS2_CFG_FIRST_CLOCK_DISABLE = (1 << 4),  ///< First PS/2 port clock disable
    PS2_CFG_SECOND_CLOCK_DISABLE = (1 << 5), ///< Second PS/2 port clock disable
    PS2_CFG_FIRST_TRANSLATION = (1 << 6),    ///< First PS/2 port translation enable
    PS2_CFG_RESERVED_7 = (1 << 7)            ///< Should be zero
} ps2_config_bits_t;

/**
 * @brief PS/2 device commands
 */
typedef enum
{
    PS2_DEV_CMD_ECHO = 0xEE,
    PS2_DEV_CMD_SET_LEDS = 0xED,
    PS2_DEV_CMD_SET_SCANCODE_SET = 0xF0,
    PS2_DEV_CMD_IDENTIFY = 0xF2,
    PS2_DEV_CMD_SET_TYPEMATIC = 0xF3,
    PS2_DEV_CMD_ENABLE_SCANNING = 0xF4,
    PS2_DEV_CMD_DISABLE_SCANNING = 0xF5,
    PS2_DEV_CMD_SET_DEFAULTS = 0xF6,
    PS2_DEV_CMD_RESEND = 0xFE,
    PS2_DEV_CMD_RESET = 0xFF,
} ps2_device_cmd_t;

/**
 * @brief PS/2 device identifiers
 */
typedef enum
{
    PS2_DEV_NONE = -1,  ///< No device
    PS2_DEV_FIRST = 0,  ///< First PS/2 port
    PS2_DEV_SECOND = 1, ///< Second PS/2 port
    PS2_DEV_COUNT = 2   ///< Total number of ports
} ps2_device_t;

/**
 * @brief PS/2 device types
 */
typedef enum
{
    PS2_DEV_TYPE_UNKNOWN,
    PS2_DEV_TYPE_KEYBOARD,
    PS2_DEV_TYPE_MOUSE_STANDARD,
    PS2_DEV_TYPE_MOUSE_SCROLL,
    PS2_DEV_TYPE_MOUSE_5BUTTON,
} ps2_device_type_t;

/**
 * @brief PS/2 device information structure
 */
typedef struct
{
    ps2_device_t device;
    uint8_t firstIdByte;
    const char* name;
    ps2_device_type_t type;
    bool active;
} ps2_device_info_t;

/**
 * @brief PS/2 controller self-test responses
 */
typedef enum
{
    PS2_SELF_TEST_PASS = 0x55,
    PS2_SELF_TEST_FAIL = 0xFC,
} ps2_self_test_response_t;

/**
 * @brief PS/2 device test responses
 */
typedef enum
{
    PS2_DEV_TEST_PASS = 0x00,
    PS2_DEV_TEST_CLOCK_STUCK_LOW = 0x01,
    PS2_DEV_TEST_CLOCK_STUCK_HIGH = 0x02,
    PS2_DEV_TEST_DATA_STUCK_LOW = 0x03,
    PS2_DEV_TEST_DATA_STUCK_HIGH = 0x04,
} ps2_device_test_response_t;

/**
 * @brief PS/2 device command responses
 */
typedef enum
{
    PS2_DEV_RESPONSE_ACK = 0xFA,
    PS2_DEV_RESPONSE_RESEND = 0xFE,
    PS2_DEV_RESPONSE_BAT_OK = 0xAA,
    PS2_DEV_RESPONSE_KBD_EXTENDED = 0xE0, ///< Indicates that the following byte is an extended scancode.
    PS2_DEV_RESPONSE_KBD_RELEASE = 0xF0,  ///< Indicates that the following byte is a key release code.
} ps2_device_response_t;

/**
 * @brief Read data from PS/2 controller
 *
 * Waits for the output buffer to be full, then reads a byte from the data port.
 *
 * @param data Pointer to store the read byte.
 * @return On success, `0`. On failure, ERR and errno is set to ETIMEOUT if timeout occurs.
 */
#define PS2_READ(data) \
    ({ \
        uint64_t result = ps2_wait_until_set(PS2_STATUS_OUT_FULL); \
        if (result != ERR) \
        { \
            *(data) = port_inb(PS2_PORT_DATA); \
        } \
        result; \
    })

/**
 * @brief Write data to PS/2 controller
 *
 * Waits for the input buffer to be empty, then writes a byte to the data port.
 *
 * @param data Byte to write.
 * @return On success, `0`. On failure, ERR and errno is set to ETIMEOUT if timeout occurs.
 */
#define PS2_WRITE(data) \
    ({ \
        uint64_t result = ps2_wait_until_clear(PS2_STATUS_IN_FULL); \
        if (result != ERR) \
        { \
            port_outb(PS2_PORT_DATA, data); \
        } \
        result; \
    })

/**
 * @brief Send a command to the PS/2 controller without reading response
 *
 * @param command Command to send.
 * @return On success, `0`. On failure, ERR and errno is set to ETIMEOUT if timeout occurs.
 */
#define PS2_CMD(command) \
    ({ \
        uint64_t result = ps2_send_cmd(command); \
        result; \
    })

/**
 * @brief Send a command to the PS/2 controller and read response
 *
 * @param command Command to send.
 * @param data Pointer to store the response byte
 * @return On success, `0`. On failure, ERR and errno is set to ETIMEOUT if timeout occurs.
 */
#define PS2_CMD_AND_READ(command, data) \
    ({ \
        uint64_t result = ps2_send_cmd(command); \
        if (result != ERR) \
        { \
            result = PS2_READ(data); \
        } \
        result; \
    })

/**
 * @brief Send a command to the PS/2 controller and write data
 *
 * @param command Command to send.
 * @param data Data to write.
 * @return On success, `0`. On failure, ERR and errno is set to ETIMEOUT if timeout occurs.
 */
#define PS2_CMD_AND_WRITE(command, data) \
    ({ \
        uint64_t result = ps2_send_cmd(command); \
        if (result != ERR) \
        { \
            result = PS2_WRITE(data); \
        } \
        result; \
    })

/**
 * @brief Send a command to a PS/2 device without reading response
 *
 * @param device Device to send command to, specified by its port.
 * @param command Command to send.
 * @return On success, `0`. On failure, ERR and errno is set to ETIMEOUT if timeout occurs.
 */
#define PS2_DEV_CMD(device, command) \
    ({ \
        uint64_t result = ps2_send_device_cmd(device, command); \
        result; \
    })

/**
 * @brief Send a command to a PS/2 device and read response
 *
 * @param device Device to send command to, specified by its port.
 * @param command Command to send.
 * @param data Pointer to store the response byte
 * @return On success, `0`. On failure, ERR and errno is set to ETIMEOUT if timeout occurs.
 */
#define PS2_DEV_CMD_AND_READ(device, command, data) \
    ({ \
        uint64_t result = ps2_send_device_cmd(device, command); \
        if (result != ERR) \
        { \
            result = PS2_READ(data); \
        } \
        result; \
    })

/**
 * @brief Send a command and a subcommand to a PS/2 device
 *
 * @param device Device to send command to, specified by its port.
 * @param command Command to send.
 * @param subCommand Subcommand to send.
 * @return On success, `0`. On failure, ERR and errno is set to ETIMEOUT if timeout occurs.
 */
#define PS2_DEV_SUB_CMD(device, command, subCommand) \
    ({ \
        uint64_t result = PS2_DEV_CMD(device, command); \
        if (result != ERR) \
        { \
            result = PS2_DEV_CMD(device, subCommand); \
        } \
        result; \
    })

/**
 * @brief Initialize the PS/2 controller
 *
 * Performs controller initialization including self-test, device detection,
 * and device initialization.
 */
void ps2_init(void);

/**
 * @brief Drain the PS/2 output buffer
 *
 * Reads and discards any data in the PS/2 output buffer.
 */
void ps2_drain(void);

/**
 * @brief Wait until status bit(s) is set
 *
 * @param status Status bit(s) to wait for.
 * @return On success, `0`. On failure, ERR and errno is set to ETIMEOUT if timeout occurs.
 */
uint64_t ps2_wait_until_set(ps2_status_bits_t status);

/**
 * @brief Wait until status bit(s) is clear
 *
 * @param status Status bit(s) to wait for.
 * @return On success, `0`. On failure, ERR and errno is set to ETIMEOUT if timeout occurs.
 */
uint64_t ps2_wait_until_clear(ps2_status_bits_t status);

/**
 * @brief Send a command to the PS/2 controller
 *
 * @param command Command to send.
 * @return On success, `0`. On failure, ERR and errno is set to ETIMEOUT if timeout occurs.
 */
uint64_t ps2_send_cmd(ps2_cmd_t command);

/**
 * @brief Send a command to a PS/2 device
 *
 * @param device Device to send command to, specified by its port.
 * @param command Command to send.
 * @return On success, `0`. On failure, ERR and errno is set to ETIMEOUT if timeout occurs.
 */
uint64_t ps2_send_device_cmd(ps2_device_t device, ps2_device_cmd_t command);

/** @} */

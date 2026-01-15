#pragma once

#include <kernel/cpu/io.h>

#include <kernel/cpu/irq.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief IBM Personal Computer/2 ports.
 * @defgroup modules_drivers_ps2 PS/2
 * @ingroup modules_drivers
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
#define PS2_COMMAND_RETRIES 10

/**
 * @brief All known PS/2 keyboard PNP IDs.
 *
 * @see https://uefi.org/sites/default/files/resources/devids%20%285%29.txt
 */
#define PS2_KEYBOARD_PNP_IDS \
    "PNP0300;PNP0301;PNP0302;PNP0303;PNP0304;PNP0305;PNP0306;PNP0307;PNP0308;PNP0309;PNP030A;PNP030B;PNP0320;PNP0321;" \
    "PNP0322;PNP0323;PNP0324;PNP0325;PNP0326;PNP0327;PNP0340;PNP0341;PNP0342;PNP0343;PNP0344"

/**
 * @brief All known PS/2 mouse PNP IDs.
 *
 * @see https://uefi.org/sites/default/files/resources/devids%20%285%29.txt
 */
#define PS2_MOUSE_PNP_IDS \
    "PNP0F00;PNP0F01;PNP0F02;PNP0F03;PNP0F04;PNP0F05;PNP0F06;PNP0F07;PNP0F08;PNP0F09;PNP0F0A;PNP0F0B;PNP0F0C;PNP0F0D;" \
    "PNP0F0E;PNP0F0F;PNP0F10;PNP0F11;PNP0F12;PNP0F13;PNP0F14;PNP0F15;PNP0F16;PNP0F17;PNP0F18;PNP0F19;PNP0F1A;PNP0F1B;" \
    "PNP0F1C;PNP0F1D;PNP0F1E;PNP0F1F;PNP0F20;PNP0F21;PNP0F22;PNP0F23;PNP0FFC;PNP0FFF"

/**
 * @brief PS/2 controller commands.
 * @enum ps2_cmd_t
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
 * @brief PS/2 controller status register bits.
 * @enum ps2_status_bits_t
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
 * @brief PS/2 controller configuration bits.
 * @enum ps2_config_bits_t
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
 * @brief PS/2 device commands.
 * @enum ps2_device_cmd_t
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
 * @brief PS/2 device identifiers.
 * @enum ps2_device_t
 */
typedef enum
{
    PS2_DEV_NONE = -1,
    PS2_DEV_FIRST = 0,
    PS2_DEV_SECOND = 1,
    PS2_DEV_COUNT = 2
} ps2_device_t;

/**
 * @brief PS/2 device types.
 * @enum ps2_device_type_t
 */
typedef enum
{
    PS2_DEV_TYPE_NONE,
    PS2_DEV_TYPE_KEYBOARD,
    PS2_DEV_TYPE_MOUSE_STANDARD,
    PS2_DEV_TYPE_MOUSE_SCROLL,
    PS2_DEV_TYPE_MOUSE_5BUTTON,
} ps2_device_type_t;

/**
 * @brief PS/2 controller self-test responses.
 * @enum ps2_self_test_response_t
 */
typedef enum
{
    PS2_SELF_TEST_PASS = 0x55,
    PS2_SELF_TEST_FAIL = 0xFC,
} ps2_self_test_response_t;

/**
 * @brief PS/2 device test responses.
 * @enum ps2_device_test_response_t
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
 * @brief PS/2 device command responses.
 * @enum ps2_device_response_t
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
 * @brief Known PS/2 device structure.
 * @struct ps2_known_device_t
 */
typedef struct
{
    const char* pnpId;
    const char* name;
} ps2_known_device_t;

/**
 * @brief PS/2 device information structure.
 * @struct ps2_device_info_t
 */
typedef struct
{
    ps2_device_t device; ///< Device port.
    const char* pnpId;   ///< PNP ID of the device.
    const char* name;    ///< Human-readable name of the device.
    irq_virt_t irq;      ///< IRQ assigned to the device by ACPI.
    bool attached;       ///< The device has been attached from ACPI.
    bool initialized;    ///< The device has been initialized.
    void* data;       ///< Driver-specific private data.
} ps2_device_info_t;

/**
 * @brief Drain the PS/2 output buffer.
 *
 * Reads and discards any data in the PS/2 output buffer.
 */
void ps2_drain(void);

/**
 * @brief Wait until status bit(s) are set.
 *
 * @param status Status bit(s) to wait for.
 * @return On success, `0`. On timeout, `ERR`.
 */
uint64_t ps2_wait_until_set(ps2_status_bits_t status);

/**
 * @brief Wait until status bit(s) are clear.
 *
 * @param status Status bit(s) to wait for.
 * @return On success, `0`. On timeout, `ERR`.
 */
uint64_t ps2_wait_until_clear(ps2_status_bits_t status);

/**
 * @brief Read from the PS/2 controllers data port.
 *
 * Waits for the output buffer to be full, then reads a byte from the data port.
 *
 * @return On success, the response byte. On failure, `ERR` and `errno` is set to:
 * - `ETIMEOUT`: Timeout occurred while waiting for data.
 */
uint64_t ps2_read(void);

/**
 * @brief Read from the PS/2 controllers data port without waiting.
 *
 * @return On success, the response byte. On failure, `ERR` and `errno` is set to:
 * - `EAGAIN`: No data available to read.
 */
uint64_t ps2_read_no_wait(void);

/**
 * @brief Write to the PS/2 controllers data port.
 *
 * Waits for the input buffer to be empty, then writes a byte to the data port.
 *
 * @param data Byte to write.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `ETIMEOUT`: Timeout occurred while waiting to write.
 */
uint64_t ps2_write(uint8_t data);

/**
 * @brief Send a command to the PS/2 controller without reading response.
 *
 * @param command Command to send.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `ETIMEOUT`: Timeout occurred while waiting to send command.
 */
uint64_t ps2_cmd(ps2_cmd_t command);

/**
 * @brief Send a command to the PS/2 controller and read response.
 *
 * @param command Command to send.
 * @return On success, the response byte. On failure, `ERR` and `errno` is set to:
 * - `ETIMEOUT`: Timeout occurred while waiting to send command or read data.
 */
uint64_t ps2_cmd_and_read(ps2_cmd_t command);

/**
 * @brief Send a command to the PS/2 controller and write data
 *
 * @param command Command to send.
 * @param data Data to write.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `ETIMEOUT`: Timeout occurred while waiting to send command or write data.
 */
uint64_t ps2_cmd_and_write(ps2_cmd_t command, uint8_t data);

/**
 * @brief Send a command to a PS/2 device without reading response.
 *
 * @param device Device to send command to.
 * @param command Command to send.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `ETIMEOUT`: Timeout occurred while waiting to send command.
 */
uint64_t ps2_device_cmd(ps2_device_t device, ps2_device_cmd_t command);

/**
 * @brief Send a command to a PS/2 device and read response.
 *
 * @param device Device to send command to, specified by its port.
 * @param command Command to send.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `ETIMEOUT`: Timeout occurred while waiting to send command or read data.
 */
uint64_t ps2_device_cmd_and_read(ps2_device_t device, ps2_device_cmd_t command);

/**
 * @brief Send a command and a subcommand to a PS/2 device.
 *
 * @param device Device to send command to, specified by its port.
 * @param command Command to send.
 * @param subCommand Subcommand to send.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `ETIMEOUT`: Timeout occurred while waiting to send command or subcommand.
 */
uint64_t ps2_device_sub_cmd(ps2_device_t device, ps2_device_cmd_t command, uint8_t subCommand);

/** @} */

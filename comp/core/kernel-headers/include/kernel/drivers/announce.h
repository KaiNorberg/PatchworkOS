#pragma once

#include <kernel/acpi/devices.h>
#include <kernel/fs/file.h>
#include <kernel/fs/path.h>
#include <kernel/module/symbol.h>
#include <kernel/utils/ref.h>
#include <kernel/version.h>

#include <libc/fs.h>
#include <libc/list.h>
#include <libc/map.h>
#include <libc/status.h>
#include <stdint.h>

/**
 * @brief Device announcement.
 * @defgroup kernel_drivers_announce Device Announcement
 * @ingroup kernel_drivers
 *
 * The kernel and already loaded kernel modules are able to announce changes in the state of devices. This is done by
 * calling `announce_device()` which will queue a string to be read by user space in the `/dev/announce` file described
 * below.
 *
 * ## /dev/announce
 *
 * The `/dev/announce` file is used to announce device state changes to user space. When a device is attached or
 * detached, a message is written to this file. User space daemons can read this file to react to hardware changes, such
 * as loading the appropriate drivers.
 *
 * Each message is formatted as a series of whitespace deliminated strings, with each message ending with a newline
 * (`\n`).
 *
 * The first string is a timestamp in nanoseconds since boot.
 * The second string is the announcement type, which can currently be `attach` or `detach`.
 * The third string is the device type (e.g., `PNP0003` or `BOOT_RSDP`).
 * The fourth string is the compatible device type (e.g., `PNP0303`), or `-` if none is available.
 * The fifth string is the unique device name or identifier, (e.g., `_SB.PCI0.LPC0.RTC`).
 *
 * Note that in the future a message might have additional fields, as such the `\n` character should always be used to
 * find the end of a message. The order and meaning of existing fields will remain unchanged.
 *
 * @note The compat device type is to allow for generic driver loading. As an example, we might have a driver for the
 * exact device being announced, in which case the normal device type is enough. However, even if we dont have a driver
 * for that exact device, a generic driver for similar devices may suffice, this is the compat device type.
 *
 * @{
 */

/**
 * @brief Device announcement type.
 * @typedef announce_type_t
 */
typedef enum
{
    ANNOUNCE_ATTACH = 0 << 0,
    ANNOUNCE_DETACH = 1 << 0,
} announce_change_t;

/**
 * @brief Initialize the device announcement system.
 */
void announce_init(void);

/**
 * @brief Notify user space of a device state change.
 *
 * Will push a new device event to the `/dev/announce` file.
 *
 * @param type The device type string.
 * @param compat The compatible device type string, or `NULL` if not applicable.
 * @param name The device name string that uniquely identifies the device.
 * @param change The type of state change.
 * @return An appropriate status value.
 */
status_t announce_device(const char* type, const char* compat, const char* name, announce_change_t change);

/** @} */
#pragma once

#include <stdint.h>
#include <sys/io.h>

/**
 * @brief Device and Power Management
 * @defgroup kernel_acpi_devices Devices
 * @ingroup kernel_acpi
 *
 * Handles enumeration and configuration of ACPI devices, along with dynamic loading of device drivers based on ACPI
 * IDs.
 *
 * TODO: Implement hotplugging support.
 *
 * @see [PNP ACPI Registry](https://uefi.org/PNP_ACPI_Registry) for a list of known ACPI IDs.
 * 
 * @{
 */

/**
 * @brief Flags for the _STA method.
 * @enum acpi_sta_flags_t
 *
 * @see Section 6.3.7 of the ACPI specification for more details.
 */
typedef enum
{
    ACPI_STA_PRESENT = 1 << 0,    ///< Set if the device is present
    ACPI_STA_ENABLED = 1 << 1,    ///< Set if the device is enabled and decoding its resources
    ACPI_STA_SHOW_IN_UI = 1 << 2, ///< Set if the device should be shown in the UI.
    ACPI_STA_FUNCTIONAL =
        1 << 3, ///< Set if the device is functioning properly (cleared if device failed its diagnostics)
    ACPI_STA_BATTERY_PRESENT = 1 << 4, ///< Set if a battery is present
} acpi_sta_flags_t;

/**
 * @brief Default _STA flags if the _STA method does not exist.
 *
 * If the _STA method does not exist, the device is assumed to be present, enabled, shown in the UI and functioning.
 *
 * @see Section 6.3.7 of the ACPI specification for more details.
 */
#define ACPI_STA_FLAGS_DEFAULT (ACPI_STA_PRESENT | ACPI_STA_ENABLED | ACPI_STA_SHOW_IN_UI | ACPI_STA_FUNCTIONAL)

/**
 * @brief Enumerate and configure ACPI devices.
 *
 * This function always evaluates the \_SB._INI node if it exists, enumerates ACPI devices (found under \_SB), evaulates
 * their _STA object retrieving its present and functional status (if it exists) and then evaluates their _INI object
 * acording to these rules:
 * - If the _INI object does not exist it is ignored.
 * - If the _STA object does not exist the device is assumed to be present and functional
 * - If the _STA object does exist its status is read.
 * - Depending on the status returned by _STA or assumed, the device is treated in one of four ways:
 *  - If the device is not present and not functional, the device is ignored.
 *  - If the device is not present and functional, the device's _INI is ignored but its children are enumerated.
 *  - If the device is present and not functional, the device's _INI is evaluated and its children are enumerated.
 *  - If the device is present and functional, the device's _INI is evaluated and its children are enumerated.
 *
 * @see Section 6.5.1 of the ACPI specification for more details.
 */
void acpi_devices_init(void);

/** @} */

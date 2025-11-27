#pragma once

#include <modules/acpi/aml/object.h>
#include <modules/acpi/resources.h>
#include <kernel/cpu/io.h>
#include <kernel/cpu/irq.h>

#include <sys/io.h>

/**
 * @brief Device and Power Management
 * @defgroup kernel_acpi_devices Devices
 * @ingroup kernel_acpi
 *
 * Handles enumeration and configuration of ACPI devices, along with dynamic loading of device drivers.
 *
 * Each device found under the `\_SB` namespace with a `_HID` method will have its HID collected and the module system
 * will be notified that a device with that HID exists, if there is no module supporting that HID then the devices
 * `_CID` method will be evaluated (if it exists) and the module system will be notified of the CID returned by that
 * method.
 *
 * Processor Container Devices (HID "ACPI0010") are ignored as their use is deprecated so even if the hardware provides
 * them, we dont want to use them. Its also possible that certain devices such as the HPET are not reported even if they
 * exist, in these cases we manually check for them and add their HIDs.
 *
 * ## Hardware IDs (HIDs) and Compatible IDs (CIDs)
 *
 * The difference between HIDs and CIDs is that HIDs are unique identifiers for the specific device type, while CIDs are
 * more generic identifiers. Its the difference between a specific model of network card and just a generic network
 * card.
 *
 * Trying HIDs first and CIDs after means we try to load a module for the exact device, or if that fails a generic
 * module that can handle the device, tho perhaps not optimally.
 *
 * ## Device Configuration
 *
 * Each ACPI device specifies the resources it needs via its AML, for example via the `_CRS` method. This can include
 * IRQs, IO ports, etc. During device initialization, this data is parsed and the necessary resources are allocated and
 * configured for the device.
 *
 * ## Module Loading and Device Configuration Order
 *
 * For the sake of ensuring consistency across different systems, all modules will be loaded based on their ACPI
 * HIDs or CIDs in alphanumerical order, this also applies to device configuration. This means that a device with the
 * ACPI HID "ACPI0001" will be loaded before a device with the ACPI HID "ACPI0002" and that one before the device with
 * the ACPI HID "PNP0000". This only applies to the module loading and device configuration but not to device
 * enumeration.
 *
 * TODO: Implement hotplugging support.
 *
 * @see [PNP ACPI Registry](https://uefi.org/PNP_ACPI_Registry) for a list of known ACPI HIDs.
 * @see Section 6.1.2 and 6.1.5 of the ACPI specification for more details on HIDs and CIDs.
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
 * @brief Represents a IRQ assigned to an ACPI device.
 * @struct acpi_device_irq_t
 */
typedef struct acpi_device_irq
{
    irq_phys_t phys;
    irq_virt_t virt;
    irq_flags_t flags;
} acpi_device_irq_t;

/**
 * @brief Represents an IO port range assigned to an ACPI device.
 * @struct acpi_device_io_t
 */
typedef struct acpi_device_io
{
    port_t base;
    uint64_t length;
} acpi_device_io_t;

/**
 * @brief ACPI device configuration structure.
 * @struct acpi_device_cfg_t
 *
 * Stores the resources assigned to an ACPI device, like IRQs and IO ports.
 *
 * TODO: Add more config stuff like memory ranges, DMA etc.
 */
typedef struct acpi_device_cfg
{
    char hid[MAX_NAME];
    char cid[MAX_NAME];
    acpi_device_irq_t* irqs;
    uint64_t irqCount;
    acpi_device_io_t* ios;
    uint64_t ioCount;
} acpi_device_cfg_t;

/**
 * @brief Enumerate, configure and load modules for ACPI devices.
 *
 * This function always evaluates the \_SB._INI node if it exists, enumerates ACPI devices (found under \_SB), evaluates
 * their _STA object retrieving its present and functional status (if it exists) and then evaluates their _INI object
 * according to these rules:
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
 * @return On success, 0. On failure, `ERR`.
 */
uint64_t acpi_devices_init(void);

/**
 * @brief Retrieves the ACPI device configuration for a device by its name.
 *
 * @param name The name of the device to retrieve the configuration for.
 * @return On success, a pointer to the device configuration. On failure, `NULL` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENOENT`: The specified name does not exist in the ACPI namespace.
 * - `ENOTTY`: The specified name is not a device.
 * - `ENODEV`: The specified device has no configuration.
 */
acpi_device_cfg_t* acpi_device_cfg_lookup(const char* name);

/**
 * @brief Retrieves an the nth IO port assigned to an ACPI device.
 * 
 * Usefull as the each io entry contains a base and length, making it more complex to, for example, just get port "5".
 * 
 * @param cfg The device configuration to retrieve the port from.
 * @param index The index of the IO port to retrieve.
 * @param out Output pointer to store the retrieved port.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENOSPC`: The specified index is out of bounds.
 */
uint64_t acpi_device_cfg_get_port(acpi_device_cfg_t* cfg, uint64_t index, port_t* out);

/** @} */

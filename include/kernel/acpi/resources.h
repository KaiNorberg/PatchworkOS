#pragma once

#include <stdint.h>
#include <kernel/defs.h>

/**
 * @brief ACPI resource settings.
 * @defgroup kernel_acpi_resources Resources
 * @ingroup kernel_acpi
 * 
 * In the AML namespace heirarchy each device uses a buffer object, usually returned by their `_CRS` method, to describe the resources they require, for example IO ports, IRQs, DMA channels, etc.
 * 
 * ## Example
 * 
 * So, lets take a PS2 keyboard as an example. The PS2 keyboard device will have `_CRS` method that when evaluated will return a buffer object. This buffer object will contain data in the format outlined by the structures within this section, which describe the IO ports and the IRQ that the keyboard expects to use, most likely IO ports `0x60` and `0x64` and IRQ `1`. Much more could be described, like IRQ trigger modes, polarity, DMA channels, etc. but this is a simple example.
 * 
 * ## Resource Data Format
 * 
 * The resource data is made up of a series of resource descriptors of varying formats and lengths. All descriptor types are either "small" or "large", depending on the value of the first byte of the descriptor, which decides the header used by the descriptor. After the header comes the actual data for the descriptor, which is descriptor specific, finally either another descriptor follows or the end of the resource data is reached, indicated by the "End Tag" descriptor.
 * 
 * @see Section 6.4 of the ACPI specification for more details.
 * 
 * @{
 */

/**
 * @brief ACPI resources structure.
 * @struct acpi_resources_t
 */
typedef struct
{
    uint64_t length;
    uint8_t data[];
} acpi_resources_t;

/**
 * @brief ACPI small resource header.
 * @struct acpi_resource_small_t
 * 
 * Note that the `isLarge` field is in the same position as the `isLarge` field in the large resource header.
 */
typedef struct PACKED
{
    uint8_t length : 3; ///< Does not include the header byte.
    uint8_t itemName : 4; ///< acpi_small_item_name_t
    uint8_t isLarge : 1; ///< Always 0 for small resource types.
} acpi_resource_small_t;

/**
 * @brief ACPI small resource item names.
 * @enum acpi_small_item_name_t
 */
typedef enum
{
    ACPI_SMALL_ITEM_IRQ = 0x04,
    ACPI_SMALL_ITEM_IO_PORT = 0x08,
    ACPI_SMALL_ITEM_END_TAG = 0x0F,
} acpi_small_item_name_t;

/**
 * @brief ACPI large resource header.
 * @struct acpi_resource_large_t
 * 
 * Note that the `isLarge` field is in the same position as the `isLarge` field in the small resource header.
 */
typedef struct PACKED
{
    uint8_t itemName : 7;
    uint8_t isLarge : 1; ///< Always 1 for large resource types.
    uint16_t length;
} acpi_resource_large_t;

/**
 * @brief ACPI end tag resource descriptor.
 * @struct acpi_end_tag_t
 * 
 * Found at the end of a resource settings buffer.
 */
typedef struct PACKED
{
    acpi_resource_small_t header;
    uint8_t checksum; ///< Checksum to ensure that the sum of all bytes in the resource data is zero.
} acpi_end_tag_t;

/**
 * @brief ACPI IRQ resource descriptor.
 * @struct acpi_irq_descriptor_t
 * 
 * Describes an IRQ used by the device.
 * 
 * The `info` field may or may not be present depending on the value of the `length` field in the header, if the length is 3, the `info` field is present, if the length is 2, it is not.
 */
typedef struct PACKED
{
    acpi_resource_small_t header;
    uint16_t mask; ///< Mask of IRQs used by the device, bit 0 = IRQ 0, bit 1 = IRQ 1, etc. Only one bit will be set.
    uint8_t info; ///< Optional information about the IRQ.
} acpi_irq_descriptor_t;

/**
 * @brief ACPI IO port resource descriptor.
 * @struct acpi_io_port_descriptor_t
 * 
 * Used by a device to request IO port resources with some constraints, like alignment and address range.
 * 
 * Certain legacy devices, like the PS/2 controller, will have fixed IO port addresses and will set the `minBase` and `maxBase` fields set to the same value.
 * 
 * ## Port Reservation Rules
 * 
 * The `minBase` and `maxBase` fields defines the min and maximum starting address of the IO port range, not the entire range. For example, if a device requires 8 IO ports and has `minBase = 0x10` and `maxBase = 0x20` then allocating ports `0x20` to `0x27` would be valid. 
 * 
 * The `alignment` field defines the alignment of the starting address of the IO port range. For example, if a device requires 8 IO ports and has `alignment = 0x08`, then valid starting addresses would be `0x00`, `0x08`, `0x10`, `0x18`, etc.
 * 
 * The `length` field defines the number of contiguous IO ports required by the device.
 * 
 * Finally, the `decode16` field defines whether the device uses 10-bit or 16-bit IO port decoding. As in, if it will only consider the lower 10 bits of the IO port address, or all 16 bits.
 */
typedef struct PACKED
{
    acpi_resource_small_t header;
    uint8_t decode16 : 1; ///< 0 = 10-bit decoding, 1 = 16-bit decoding.
    uint8_t reserved : 7;
    uint16_t minBase; ///< Minimum base IO port address that may be used for the device.
    uint16_t maxBase; ///< Maximum base IO port address that may be used for the device.
    uint8_t alignment; ///< Alignment of the IO port(s) to utilize within the min and max range.
    uint8_t length; ///< The amount of contiguous IO ports required by the device.
} acpi_io_port_descriptor_t;

/**
 * @brief Get the current ACPI resource settings for a device by its path.
 * 
 * @param path The device path in the AML namespace, for example "\_SB_.PCI0.SF8_.KBD_".
 * @return On success, a allocated resources structure. On failure, `NULL` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENODEV`: The device was not found or has no `_CRS` method.
 * - `EILSEQ`: Unexpected data from the `_CRS` method.
 * - `ENOMEM`: Out of memory.
 * - Other values from `aml_evaluate()`.
 */
acpi_resources_t* acpi_resources_current(const char* path);

/** @} */
#pragma once

#include <modules/acpi/aml/encoding/arg.h>
#include <stdint.h>
#include <sys/defs.h>

/**
 * @brief ACPI resource settings.
 * @defgroup modules_acpi_resources Resources
 * @ingroup modules_acpi
 *
 * In the AML namespace heirarchy each device uses a buffer object, usually returned by their `_CRS` method, to describe
 * the resources they require, for example IO ports, IRQs, DMA channels, etc.
 *
 * For the vast majority of use cases, its recommended to use the device abstraction layer provided by the `devices.h`
 * file or @ref modules_acpi_devices instead of directly parsing these overcomplicated structures.
 *
 * ## Example
 *
 * So, lets take a PS2 keyboard as an example. The PS2 keyboard device will have `_CRS` method that when evaluated will
 * return a buffer object. This buffer object will contain data in the format outlined by the structures within this
 * section, which describe the IO ports and the IRQ that the keyboard expects to use, most likely IO ports `0x60` and
 * `0x64` and IRQ `1`. Much more could be described, like IRQ trigger modes, polarity, DMA channels, etc. but this is a
 * simple example.
 *
 * ## Resource Data Format
 *
 * The resource data is made up of a series of resource descriptors of varying formats and lengths. All descriptor types
 * are either "small" or "large", depending on the value of the first byte of the descriptor, which decides the header
 * used by the descriptor. After the header comes the actual data for the descriptor, which is descriptor specific,
 * finally either another descriptor follows or the end of the resource data is reached, indicated by the "End Tag"
 * descriptor.
 *
 * ## Helpers
 *
 * Since the resource descriptor format is kinda messy, a abstraction layer is provided in the form of helper macros
 * which should always be used instead of directly parsing the resource data.
 *
 * @see Section 6.4 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief ACPI small resource header.
 * @struct acpi_resource_small_t
 *
 * Note that the `isLarge` field is in the same position as the `isLarge` field in the large resource header.
 */
typedef struct PACKED
{
    uint8_t length : 3;   ///< Does not include the header byte.
    uint8_t itemName : 4; ///< acpi_item_name_small_t
    uint8_t isLarge : 1;  ///< Always 0 for small resource types.
} acpi_resource_small_t;

/**
 * @brief ACPI large resource header.
 * @struct acpi_resource_large_t
 *
 * Note that the `isLarge` field is in the same position as the `isLarge` field in the small resource header.
 */
typedef struct PACKED
{
    uint8_t itemName : 7; ///< acpi_item_name_large_t
    uint8_t isLarge : 1;  ///< Always 1 for large resource types.
    uint16_t length;      ///< Does not include the header bytes.
} acpi_resource_large_t;

/**
 * @brief ACPI IRQ resource descriptor.
 * @struct acpi_irq_descriptor_t
 *
 * @note The actual IRQ resource descriptor may contain an additional byte at the end depending on if the `length` field
 * in the header is `2` or `3`. To maintain type safety we do not include this byte in the structure. Instead use the
 * `ACPI_IRQ_DESCRIPTOR_INFO()` macro to access it if needed.
 */
typedef struct PACKED
{
    acpi_resource_small_t header;
    uint16_t mask; ///< Mask of IRQs used by the device, bit 0 = IRQ 0, bit 1 = IRQ 1, etc..
} acpi_irq_descriptor_t;

/**
 * @brief ACPI IRQ descriptor info flags.
 * @enum acpi_irq_descriptor_info_t
 *
 * Stored in the optional third byte of the IRQ resource descriptor, if the third byte is not present then assume "edge
 * sensitive, high true interrupts", as in all zeroes.
 */
typedef enum
{
    ACPI_IRQ_LEVEL_TRIGGERED = 0 << 0, ///< Interrupt is triggered in response to signal in a low state.
    ACPI_IRQ_EDGE_TRIGGERED =
        1 << 0,                    ///< Interrupt is triggered in response to a change in signal state from low to high.
    ACPI_IRQ_ACTIVE_HIGH = 0 << 3, ///< This interrupt is sampled with the signal is high, or true.
    ACPI_IRQ_ACTIVE_LOW = 1 << 3,  ///< This interrupt is sampled with the signal is low, or false.
    ACPI_IRQ_SHARED = 0 << 4,      ///< This interrupt is shared with other devices.
    ACPI_IRQ_EXCLUSIVE = 1 << 4,   ///< This interrupt is not shared with other devices.
    ACPI_IRQ_NOT_WAKE_CAPABLE = 0 << 5, ///< This interrupt is not capable of waking the system.
    ACPI_IRQ_WAKE_CAPABLE =
        1 << 5, ///< This interrupt is capable of waking the system from a low-power idle state or a system sleep state.
    ACPI_IRQ_RESERVED1 = 1 << 6,
    ACPI_IRQ_RESERVED2 = 1 << 7,
} acpi_irq_descriptor_info_t;

/**
 * @brief Retrieves the IRQ descriptor info flags from an IRQ resource descriptor.
 *
 * Will assume all zeroes if the optional third byte is not present.
 *
 * @param descriptor Pointer to an `acpi_irq_descriptor_t` structure.
 * @return The IRQ descriptor info flags as an `acpi_irq_descriptor_info_t` value.
 */
#define ACPI_IRQ_DESCRIPTOR_INFO(descriptor) \
    ((acpi_irq_descriptor_info_t)((descriptor)->header.length >= 3 \
            ? *((((uint8_t*)(descriptor)) + sizeof(acpi_irq_descriptor_t))) \
            : 0))

/**
 * @brief ACPI IO port resource descriptor.
 * @struct acpi_io_port_descriptor_t
 *
 * Used by a device to request IO port resources with some constraints, like alignment and address range.
 *
 * ## Port Reservation Rules
 *
 * The `minBase` and `maxBase` fields defines the min and maximum starting address of the IO port range, not the entire
 * range. For example, if a device requires 8 IO ports and has `minBase = 0x10` and `maxBase = 0x20` then allocating
 * ports `0x20` to `0x27` would be valid.
 *
 * The `alignment` field defines the alignment of the starting address of the IO port range. For example, if a device
 * requires 8 IO ports and has `alignment = 0x08`, then valid starting addresses would be `0x00`, `0x08`, `0x10`,
 * `0x18`, etc.
 *
 * The `length` field defines the number of contiguous IO ports required by the device.
 *
 * Finally, the `decode16` field defines whether the device uses 10-bit or 16-bit IO port decoding. As in, if it will
 * only consider the lower 10 bits of the IO port address, or all 16 bits.
 *
 * @note Certain legacy devices, like the PS/2 controller, will have fixed IO port addresses and will set the `minBase`
 * and `maxBase` fields set to the same value.
 */
typedef struct PACKED
{
    acpi_resource_small_t header;
    uint8_t decode16 : 1; ///< 0 = 10-bit decoding, 1 = 16-bit decoding.
    uint8_t reserved : 7;
    uint16_t minBase;  ///< Minimum base IO port address that may be used for the device.
    uint16_t maxBase;  ///< Maximum base IO port address that may be used for the device.
    uint8_t alignment; ///< Alignment of the IO port(s) to utilize within the min and max range.
    uint8_t length;    ///< The amount of contiguous IO ports required by the device.
} acpi_io_port_descriptor_t;

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
 * @brief Small ACPI resource item names.
 * @enum acpi_item_name_small_t
 *
 * This enum stores the values that will be found in the actual small resource descriptor headers.
 */
typedef enum
{
    ACPI_ITEM_SMALL_IRQ = 0x04,
    ACPI_ITEM_SMALL_IO_PORT = 0x08,
    ACPI_ITEM_SMALL_END_TAG = 0x0F,
} acpi_item_name_small_t;

/**
 * @brief Large ACPI resource item names.
 * @enum acpi_item_name_large_t
 *
 * This enum stores the values that will be found in the actual large resource descriptor headers.
 */
typedef enum
{
    ACPI_LARGE_ITEM_24_MEM_RANGE = 0x01,
} acpi_item_name_large_t;

/**
 * @brief ACPI resources structure.
 * @struct acpi_resources_t
 *
 * Buffer to store all the resource descriptors for a device.
 */
typedef struct
{
    uint64_t length;
    uint8_t data[];
} acpi_resources_t;

/**
 * @brief Generic ACPI resource descriptor.
 * @struct acpi_resource_t
 *
 * Used as a intermediate structure to determine if a resource descriptor is small or large.
 */
typedef struct PACKED
{
    uint8_t reserved : 7;
    uint8_t isLarge : 1; ///< Same position in both small and large resource headers.
} acpi_resource_t;

/**
 * @brief Generic ACPI resource item names.
 * @enum acpi_item_name_t
 *
 * This enum stores the values returned by `ACPI_RESOURCE_ITEM_NAME()` macro, NOT the actual values found in the
 * resource descriptor headers.
 *
 * Used to simplify checking the type of a resource descriptor.
 */
typedef enum
{
    ACPI_ITEM_NAME_IRQ = ACPI_ITEM_SMALL_IRQ,
    ACPI_ITEM_NAME_IO_PORT = ACPI_ITEM_SMALL_IO_PORT,
    ACPI_ITEM_NAME_END_TAG = ACPI_ITEM_SMALL_END_TAG,

    ACPI_ITEM_NAME_LARGE_BASE = 0x100,
    ACPI_ITEM_NAME_24_MEM_RANGE = ACPI_ITEM_NAME_LARGE_BASE + ACPI_LARGE_ITEM_24_MEM_RANGE,
} acpi_item_name_t;

/**
 * @brief Helper macro to get the generic item name of a resource descriptor.
 *
 * Abstracts away the difference between small and large resource descriptors, after this any resource can be type cast
 * to the expected structure based on the returned item name.
 *
 * @param resource Pointer to an `acpi_resource_t` structure.
 * @return The item name of the resource descriptor as an `acpi_item_name_t` value.
 */
#define ACPI_RESOURCE_ITEM_NAME(resource) \
    (((resource)->isLarge) \
            ? (acpi_item_name_t)(ACPI_ITEM_NAME_LARGE_BASE + ((acpi_resource_large_t*)(resource))->itemName) \
            : (acpi_item_name_t)((acpi_resource_small_t*)(resource))->itemName)

/**
 * @brief Helper macro to get the size of a resource descriptor.
 *
 * @param resource Pointer to an `acpi_resource_t` structure.
 * @return The size of the entire resource descriptor, including the header.
 */
#define ACPI_RESOURCE_SIZE(resource) \
    (((resource)->isLarge) ? ((acpi_resource_large_t*)(resource))->length + sizeof(acpi_resource_large_t) \
                           : ((acpi_resource_small_t*)(resource))->length + sizeof(acpi_resource_small_t))

/**
 * @brief Helper macro to iterate over all resource descriptors in an ACPI resources structure.
 *
 * Works by initializing a pointer to the start of the resource data and then iterating over each descriptor by checking
 * if its a small or large descriptor, retreving its size, and moving the pointer forward by that size.
 *
 * @param resource Pointer to an `acpi_resource_t` structure that will be set to each resource descriptor in the
 * iteration.
 * @param resources The `acpi_resources_t` structure to iterate over.
 */
#define ACPI_RESOURCES_FOR_EACH(resource, resources) \
    for (uint8_t* __ptr = (resources)->data; \
        (__ptr < (resources)->data + (resources)->length) && ((resource) = (acpi_resource_t*)__ptr) && \
        (__ptr + ACPI_RESOURCE_SIZE(resource) <= (resources)->data + (resources)->length); \
        __ptr += ACPI_RESOURCE_SIZE(resource))

/**
 * @brief Get the current ACPI resource settings for a device.
 *
 * Will ensure the data return by the device's `_CRS` method is valid, no need for the caller to do so.
 *
 * @param device The device object in the AML namespace.
 * @return On success, a allocated resources structure. On failure, `NULL` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENOENT`: The device has no `_CRS` method.
 * - `EILSEQ`: Unexpected data from the `_CRS` method.
 * - `ENOMEM`: Out of memory.
 * - Other values from `aml_evaluate()`.
 */
acpi_resources_t* acpi_resources_current(aml_object_t* device);

/**
 * @brief Free an ACPI resources structure.
 *
 * @param resources Pointer to the resources structure to free.
 */
void acpi_resources_free(acpi_resources_t* resources);

/** @} */
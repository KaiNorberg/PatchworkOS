#pragma once

#include <kernel/acpi/devices.h>

#include <stdint.h>
#include <sys/io.h>
#include <sys/list.h>

typedef struct module module_t;

/**
 * @brief Kernel module management.
 * @defgroup kernel_module Module Management
 * @ingroup kernel
 *
 * @{
 */

/**
 * @brief Module event types.
 * @typedef module_event_type_t
 */
typedef enum module_event_type
{
    MODULE_EVENT_NONE = 0,
    MODULE_EVENT_LOAD,
    MODULE_EVENT_UNLOAD,
} module_event_type_t;

/**
 * @brief Module event structure.
 * @typedef module_event_t
 *
 * Will be sent to modules procedure as events occur.
 */
typedef struct module_event
{
    module_event_type_t type;
    union {
        struct
        {
            acpi_hid_t hid;
        } load;
        struct
        {
            acpi_hid_t hid;
        } unload;
    };
} module_event_t;

/**
 * @brief Module structure.
 * @typedef module_t
 */
typedef struct module
{
    list_entry_t entry;
    char name[MAX_NAME];
    void* baseAddress; ///< The address where the modules image is loaded in memory.
    uint64_t size;     ///< The size of the modules loaded image in memory.
    void (*procedure)(module_event_t* event);
} module_t;

#define MODULE_ACPI_HID_TABLE_DEFINE(...) \
    static const acpi_hid_t MODULE_ACPI_HID_TABLE[] = {__VA_ARGS__}; \
    static const size_t MODULE_ACPI_HID_TABLE_SIZE = sizeof(MODULE_ACPI_HID_TABLE) / sizeof(acpi_hid_t);

/**
 * @brief Initializes the module system.
 */
void module_init(void);

/**
 * @brief Propagates a module event to all registered modules.
 *
 * If the event is a load or unload event, then the event's `hid` field will be matched against all modules' HID tables,
 * if a match is found that module will be loaded/unloaded.
 *
 * @param event The event to propagate.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t module_event(module_event_t* event);

/** @} */

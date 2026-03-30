#pragma once

#include <kernel/fs/dentry.h>
#include <kernel/module/symbol.h>
#include <libc/fs.h>
#include <libc/list.h>
#include <libc/status.h>

/**
 * @brief Kernel module management.
 * @defgroup kernel_module Module Management
 * @ingroup kernel
 *
 * A module is a dynamically loadable kernel object that can extend the kernel's functionality at runtime by, for
 * example, implementing drivers or IPC objects.
 *
 * ## /sys/mod/load
 *
 * Loading modules is handled by writing a raw ELF buffer to the `/sys/mod/load` file.
 *
 * This ELF buffer should be prepended by three NULL-terminated strings, the first string being the name of the module,
 * the second being device type and the third being the device name.
 *
 * If the device type string or the device name string is equal to "-", the module will be loaded but will not receive a
 * device attach event, usefull for loading dependencies.
 *
 * The kernel will then verify the provided binary, copy it and finally perform the needed relocation and linking.
 *
 * @todo Implement module unloading.
 *
 * @{
 */

/**
 * @brief Reserved prefix for module global symbols.
 *
 * Any symbol with this prefix will not be loaded or exported.
 */
#define MODULE_RESERVED_PREFIX "_mod"

/**
 * @brief Length of `MODULE_RESERVED_PREFIX`.
 */
#define MODULE_RESERVED_PREFIX_LENGTH 4

/**
 * @brief Module event types.
 * @typedef module_event_type_t
 */
typedef enum module_event_type
{
    MODULE_EVENT_NONE = 0,
    MODULE_EVENT_LOAD,
    MODULE_EVENT_UNLOAD,
    MODULE_EVENT_DEVICE_ATTACH,
    MODULE_EVENT_DEVICE_DETACH,
} module_event_type_t;

/**
 * @brief Module event structure.
 * @typedef module_event_t
 *
 * Will be sent to a modules procedure as events occur.
 */
typedef struct module_event
{
    module_event_type_t type;
    union {
        struct
        {
            const char* type;
            const char* name;
        } deviceAttach;
        struct
        {
            const char* type;
            const char* name;
        } deviceDetach;
    };
} module_event_t;

typedef status_t (*module_procedure_t)(const module_event_t* event);

/**
 * @brief Module structure.
 * @typedef module_t
 */
typedef struct module
{
    list_entry_t listEntry;
    char name[MAX_NAME];
    void* baseAddr;                  ///< The address where the modules image is loaded in memory.
    uint64_t size;                   ///< The size of the modules loaded image in memory.
    symbol_group_id_t symbolGroupId; ///< The symbol group ID for the module's symbols.
    module_procedure_t procedure;
    dentry_t* instanceDir;
    dentry_t* attachFile;
} module_t;

/**
 * @brief Expose the `/sys/mod/load` file.
 */
void module_expose(void);

/**
 * @brief Initialize a fake module representing the kernel itself.
 *
 * Will panic on failure.
 *
 * Used for symbol grouping.
 */
status_t module_init_fake_kernel_module(void);

/** @} */
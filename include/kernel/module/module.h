#pragma once

#include <kernel/acpi/devices.h>
#include <kernel/fs/file.h>
#include <kernel/utils/map.h>
#include <kernel/utils/ref.h>

#include <stdint.h>
#include <sys/io.h>
#include <sys/list.h>

typedef struct module module_t;

/**
 * @brief Kernel module management.
 * @defgroup kernel_module Module Management
 * @ingroup kernel
 *
 * A module is a dynamically loadable piece of code. This means that for example, instead of having to store every
 * possible driver into the kernel all the time we can detect what hardware is present and only load the necessary
 * modules for that hardware.
 *
 * Its also very useful just for organization purposes as it lets us seperate out our concerns, isntead of just packing
 * everything into the kernel.
 *
 * ## Writing Modules
 *
 * Modules are in effect just ELF binaries which export 2 special sections and 1 special symbol.
 *
 * The first special section is `.module_info` which contains metadata about the module. Check the `MODULE_INFO` macro
 * for more details.
 *
 * The second special section is `.module_acpi_hids` which contains a list of ACPI HIDs that the module can handle.
 * Check the `MODULE_ACPI_HIDS` macro for more details.
 *
 * Finally, there is the special symbol `_module_procedure` which can be thought of as the "main" function of the module
 * but it also does way more than just that, whenever any event occurs that the module should be aware of (like being
 * loaded or unloaded) this procedure will be called to notify the module of the event.
 *
 * TODO: There can only be one module per ACPI HID, priority system?
 *
 * @{
 */

/**
 * @brief Module event types.
 * @typedef module_event_type_t
 *
 * If a module's procedure returns `ERR` while handling a `MODULE_EVENT_INIT` event, the module will be unloaded again,
 * for all other events a warning will be logged.
 */
typedef enum module_event_type
{
    MODULE_EVENT_NONE = 0,
    MODULE_EVENT_LOAD,
    MODULE_EVENT_UNLOAD,
} module_event_type_t;

typedef enum
{
    MODULE_FLAG_NONE = 0,
    MODULE_FLAG_LOADED = 1 << 0,
} module_flags_t;

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
            const char* hid;
        } load;
        struct
        {
            const char* hid;
        } unload;
    };
} module_event_t;

/**
 * @brief Module structure.
 * @typedef module_t
 */
typedef struct module
{
    void* baseAddr; ///< The address where the modules image is loaded in memory.
    uint64_t size;  ///< The size of the modules loaded image in memory.
    uint64_t (*procedure)(module_event_t* event);
    list_t hidHandlers; ///< List of `hid_handler_t` entries for this module.
    file_t* file;
    module_flags_t flags;
} module_t;

/**
 * @brief HID handler structure.
 * @typedef hid_handler_t
 *
 * Used to assign a module to one or multiple ACPI HIDs.
 */
typedef struct
{
    list_entry_t moduleEntry;
    map_entry_t hidMapEntry;
    module_t* module;
    char hid[MAX_NAME];
} hid_handler_t;

/**
 * @brief Macro to define module information.
 *
 * To define module information we define a separate section in the module's binary called `.module_info` this section
 * stores a concatenated string of the module's name, author, description, version and licence, each separated by a
 * null-terminator. We also add a final null-terminator and a byte `1` at the end to signify the end of the module info.
 *
 * For example
 * ```c
 * MODULE_INFO("My Module", "John Doe", "A sample module", "1.0.0", "MIT");
 * ```
 * becomes
 * ```c
 * "My Module\0John Doe\0A sample module\01.0.0\0MIT\0\1"
 * ```
 *
 * Should only be used in a module.
 *
 * @param _name The name of the module.
 * @param _author The author of the module.
 * @param _description A short description of the module.
 * @param _version The version of the module.
 * @param _licence The licence of the module.
 */
#define MODULE_INFO(_name, _author, _description, _version, _licence) \
    const char _moduleInfo[] __attribute__((section(".module_info"), used)) = \
        _name "\0" _author "\0" _description "\0" _version "\0" _licence "\0\1"

/**
 * @brief Macro to define what ACPI HIDs a module can handle.
 *
 * To define what ACPI HIDs a module can handle we define a separate section in the module's binary called
 * `.module_acpi_hids` this section stores a concatenated string of all ACPI HIDs the module can handle. We dont need
 * terminators for each string as their length is fixed depending on their prefix, we do have a null-terminator at the
 * end of the entire list though.
 *
 * For example
 * ```c
 * MODULE_ACPI_HIDS("PNP0C0A", "PNP0C0B", "ACPI0003")
 * ```
 * becomes
 * ```c
 * "PNP0C0APNP0C0BACPI0003\0"
 * ```
 *
 * Should only be used in a module.
 *
 * @see https://uefi.org/PNP_ACPI_Registry for lists of ACPI IDs.
 *
 * @param ... A variable amount of ACPI HIDs as string literals.
 */
#define MODULE_ACPI_HIDS(...) \
    static const char _moduleAcpiHids[] __attribute__((section(".module_acpi_hids"), used)) = __VA_ARGS__ "\0"

/**
 * @brief Initializes the module system.
 *
 * Will load the metadata of all modules in the `/kernel/modules` directory and register them with the module system.
 *
 * Note that this does not mean that the binary will be loaded into memory, that only happens when a module is needed,
 * it just means that the kernel reads the metadata so it knows what modules are available.
 *
 * TODO: Loading all modules like this will not scale well, in the future consider a "Linux depmod" style approach.
 */
void module_init(void);

/**
 * @brief Propagates a module event to all registered modules.
 *
 * If the event is a load or unload event, then the event's `hid` field will be matched against all modules' HIDs,
 * if a match is found that module will be loaded/unloaded.
 *
 * @param event The event to propagate.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t module_event(module_event_t* event);

/** @} */

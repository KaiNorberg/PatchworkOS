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
 * Modules are in effect just ELF binaries which export some special sections and symbols that the kernel module loader can use to identify and load the module.
 *
 * The first special section is `.module_info` which contains metadata about the module. Check the `MODULE_INFO` macro
 * for more details.
 *
 * Finally, there is the special symbol `_module_procedure()` which can be thought of as the "main" function of the module
 * but it also does way more than just that, whenever any event occurs that the module should be aware of this procedure will be called to notify the module of the event.
 *
 * @{
 */

/**
 * @brief Module event types.
 * @typedef module_event_type_t
 *
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
} module_t;

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

/** */
uint64_t module_load(const char* path);

uint64_t module_unload(const char* name);

module_t* module_get(const char* name);

/** @} */

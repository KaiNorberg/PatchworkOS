#pragma once

#include <kernel/acpi/devices.h>
#include <kernel/fs/file.h>
#include <kernel/fs/path.h>
#include <kernel/utils/map.h>
#include <kernel/utils/ref.h>
#include <kernel/module/symbol.h>

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
 * Modules are in effect just ELF binaries which export some special sections and symbols that the kernel module loader
 * can use to identify and load the module.
 *
 * The first special section is `.module_info` which contains metadata about the module. Check the `MODULE_INFO` macro
 * for more details.
 *
 * Finally, there is the special symbol `module_procedure()` which can be thought of as the "main" function of the
 * module but it also does way more than just that, whenever any event occurs that the module should be aware of this
 * procedure will be called to notify the module of the event.
 *
 * Note that since all global symbols will be exposed to other modules, its a good idea to prefix all global symbols
 * with some unique prefix to avoid naming collisions with other modules, for example `mymodule_*`.
 *
 * ## Dependencies
 *
 * Modules can depend on other modules. For example, module1 could define the function `module_1_func()` and then
 * module2 could call this function. The only way for that to work is for the kernel to load module1 before or during
 * the loading of module2 so that the symbol `module_1_func()` can be resolved when module2 is being relocated.
 *
 * There are many, many ways of handling dependencies. In PatchworkOS it works like this.
 *
 * First, we load some module file, lets say "/kernel/modules/module2". This module wants to call `module_1_func()`
 * which is defined in "/kernel/modules/module1". When resolving the symbols for module2 we will fail to resolve
 * `module_1_func()`.
 *
 * The failure to resolve a symbol will cause the kernel to check all other module files in the same directory as
 * module2, in this case "/kernel/modules/", it checks all the symbols in each module eventually finding that module1
 * defines `module_1_func()`. The kernel will then load module1 and retry the symbol resolution for module2, this time
 * succeeding. This repeats until all symbols are resolved or no more modules are found to load.
 *
 * This means that both module1 and module2 need to do exactly nothing, they dont even need to declare that they depend
 * on each other, the kernel will figure it all out automatically as long as all modules are in the same directory.
 *
 * ## Circular Dependencies
 *
 * When loading a module with dependencies, circular dependencies may occur. For example, module A depends on module B
 * which in turn depends on module A.
 *
 * Our solution for this is quite simple. When loading a module we keep track of all modules that we have been loaded
 * during the current load operation. If we during dependency resolution try to load a module that is already loaded
 * then we have detected a circular dependency and the entire module load fails.
 *
 * ## Unloading Modules
 *
 * Modules can be unloaded by the kernel when they are no longer needed, for example, due to a device being removed.
 * However, there may be other modules that depend on the module being unloaded. To solve this issue, each module is
 * reference counted. When a module is depended on by another module, its reference count is incremented. When its
 * reference count reaches zero, it can be safely unloaded.
 *
 * @{
 */

#define MODULE_MAX_NAME 64
#define MODULE_MAX_AUTHOR 64
#define MODULE_MAX_DESCRIPTION 256
#define MODULE_MAX_VERSION 32
#define MODULE_MAX_LICENCE 64
#define MODULE_MAX_INFO \
    (MODULE_MAX_NAME + MODULE_MAX_AUTHOR + MODULE_MAX_DESCRIPTION + MODULE_MAX_VERSION + MODULE_MAX_LICENCE + 6)

/**
 * @brief Module event types.
 * @typedef module_event_type_t
 */
typedef enum module_event_type
{
    MODULE_EVENT_NONE = 0,
    /**
     * Received when the module is loaded.
     *
     * If the module returns `ERR`, the module load will fail.
     */
    MODULE_EVENT_LOAD,
    MODULE_EVENT_DEVICE_ATTACH,
    MODULE_EVENT_DEVICE_DETACH,
    /**
     * Received when the module is unloaded.
     *
     * Return value is ignored.
     */
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
 * @brief Module procedure and entry point.
 * @typedef module_procedure_t
 */
typedef uint64_t (*module_procedure_t)(module_event_t* event);

/**
 * @brief Module structure.
 * @typedef module_t
 */
typedef struct module
{
    ref_t ref;
    list_entry_t entry;
    void* baseAddr;                           ///< The address where the modules image is loaded in memory.
    uint64_t size;                            ///< The size of the modules loaded image in memory.
    module_procedure_t procedure;             ///< The module's procedure function and entry point.
    symbol_group_id_t symbolGroupId;        ///< See `symbol_group_id_t`.
    char name[MODULE_MAX_NAME];               ///< The name of the module, from the `.module_info` section.
    char author[MODULE_MAX_AUTHOR];           ///< The author of the module, from the `.module_info` section.
    char description[MODULE_MAX_DESCRIPTION]; ///< A short description of the module, from the `.module_info` section.
    char version[MODULE_MAX_VERSION];         ///< The version of the module, from the `.module_info` section.
    char licence[MODULE_MAX_LICENCE];         ///< The licence of the module, from the `.module_info` section.
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

/**
 * @brief Load a module from the given path.
 *
 * Will also load any dependencies the module has by checking all other module files located in the same directory as
 * the module being loaded.
 *
 * @param directory The directory containing the module file.
 * @param filename The module file name.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t module_load(const char* directory, const char* filename);

/** @} */

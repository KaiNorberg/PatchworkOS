#pragma once

#include <kernel/acpi/devices.h>
#include <kernel/fs/file.h>
#include <kernel/fs/path.h>
#include <kernel/module/symbol.h>
#include <kernel/utils/map.h>
#include <kernel/utils/ref.h>
#include <kernel/version.h>

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
 * Its also very useful just for organization purposes as it lets us separate out our concerns, instead of just packing
 * everything into the kernel.
 *
 * For the record, this is a rather complex system, and in most cases you wont need to understand every facet of it to
 * use it effectively.
 *
 * ## Writing Modules
 *
 * Modules are in effect just ELF binaries which export some special sections and symbols that the kernel module loader
 * can use to identify and load the module.
 *
 * The first special section is `._module_info` which contains metadata about the module. Check the `MODULE_INFO` macro
 * for more details.
 *
 * The second special section is `._module_device_ids` which contains a semicolon-separated list of device IDs that specify
 * what devices the module can handle. Check the `MODULE_DEVICE_IDS` macro for more details.
 *
 * Finally, there is the entry point, defined by the module linker script as `_module_procedure()`, which can be thought
 * of as the "main" function of the module but it also does way more than just that, whenever any event occurs that the
 * module should be aware of this procedure will be called to notify the module of the event.
 *
 * Note that since all global symbols will be exposed to other modules, its a good idea to prefix all global symbols
 * with some unique prefix to avoid naming collisions with other modules, for example `mymodule_*`. The exception to
 * this is symbols starting with '_mod*' which will not be exported or visible to other modules.
 *
 * ## Dependencies
 *
 * Modules can depend on other modules. For example, module1 could define the function `module_1_func()` and then
 * module2 could call this function. The only way for that to work is for the kernel to load module1 before or during
 * the loading of module2 so that the symbol `module_1_func()` can be resolved when module2 is being relocated.
 *
 * There are many, many ways of handling dependencies. In PatchworkOS it works like this.
 *
 * First, we load some module file, lets say "/kernel/modules/<OS_VERSION>/module2". This module wants to call `module_1_func()`
 * which is defined in "/kernel/modules/<OS_VERSION>/module1". When resolving the symbols for module2 we will fail to resolve
 * `module_1_func()`.
 *
 * The failure to resolve a symbol will cause the kernel to check all other module files in the same directory as
 * module2, in this case "/kernel/modules/<OS_VERSION>/", it checks all the symbols in each module eventually finding that module1
 * defines `module_1_func()`. The kernel will then load module1 and retry the symbol resolution for module2, this time
 * succeeding. This repeats until all symbols are resolved or no more modules are found to load.
 *
 * This means that both module1 and module2 need to do exactly nothing, they dont even need to declare that they depend
 * on each other, the kernel will figure it all out automatically as long as all modules are in the same directory.
 *
 * Note that if a module was loaded as a dependency and all modules depending on it are unloaded, the
 * dependency module will also be unloaded, unless it was later explicitly loaded, and if a module was loaded explicitly
 * but later a module depending on it is loaded then it will also wait to be unloaded until all modules depending on it
 * are unloaded.
 *
 * TODO: In the future, there will need to be some form of cache for knowing what modules provide what symbols to avoid
 * having to scan all modules in the directory every time a symbol cant be resolved, but considering how few modules we
 * currently have... its a non-issue for now.
 *
 * ## Circular Dependencies
 *
 * When loading a module with dependencies, circular dependencies may occur. For example, module A depends on module B
 * which in turn depends on module A.
 *
 * This is allowed, which means that, for the sake of safety, all modules should be written in such a way that all their
 * functions can be safely called even if the module is not fully initialized yet. This should rarely make any
 * difference whatsoever.
 *
 * See "Unloading Modules" below for more details on how circular dependencies are handled during unloading.
 *
 * ## Unloading Modules
 *
 * Modules can be unloaded by the kernel when they are no longer needed, for example, due to a device being removed.
 * However, there may be other modules that depend on the module being unloaded.
 *
 * To solve both the issue of dependency tracking and circular dependency resolution, we implement a garbage collector
 * which, using the dependency map, traverses all reachable modules starting from the explicitly loaded modules. Any
 * module that is not reachable is considered unused and will be unloaded. Note that only modules not marked with the
 * `MODULE_FLAG_DEPENDENCY` flag are considered roots for the garbage collector.
 *
 * @{
 */

/**
 * @brief Maximum sizes for module string fields.
 * @typedef module_string_size_t
 */
typedef enum
{
    MODULE_MAX_NAME = 64,
    MODULE_MAX_AUTHOR = 64,
    MODULE_MAX_DESCRIPTION = 256,
    MODULE_MAX_VERSION = 32,
    MODULE_MAX_LICENCE = 64,
    MODULE_MAX_INFO = MODULE_MAX_NAME + MODULE_MAX_AUTHOR + MODULE_MAX_DESCRIPTION + MODULE_MAX_VERSION +
        MODULE_MAX_LICENCE + 5, ///< +5 for null-terminators
    MODULE_MIN_INFO = 6,        ///< Minimum size of module info section (5 null-terminators + 1 byte)
    MODULE_MAX_ID = 32,         ///< Maximum length of a device ID string.
    MODULE_MAX_ALL_IDS = 1024,  ///< Maximum length of all device ID strings combined.
} module_string_size_t;

/**
 * @brief Module information structure.
 * @typedef module_info_t
 *
 * Used to store module information from the `.module_info` section.
 */
typedef struct module_info
{
    char name[MODULE_MAX_NAME];
    char author[MODULE_MAX_AUTHOR];
    char description[MODULE_MAX_DESCRIPTION];
    char version[MODULE_MAX_VERSION];
    char licence[MODULE_MAX_LICENCE];
    char osVersion[MODULE_MAX_VERSION];
} module_info_t;

/**
 * @brief Reserved prefix for module global symbols.
 *
 * Any symbol with this prefix will not be loaded or exported.
 */
#define MODULE_RESERVED_PREFIX "_mod"

/**
 * @brief Length of `MODULE_RESERVED_PREFIX`.
 * @typedef module_reserved_prefix_length_t
 */
typedef enum
{
    MODULE_RESERVED_PREFIX_LENGTH = 4
} module_reserved_prefix_length_t;

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
    /**
     * Received when the module is unloaded.
     *
     * Return value is ignored.
     */
    MODULE_EVENT_UNLOAD,
    /**
     * @brief Device attach event.
     *
     * This event is sent when a device is attached that the module specified it supports.
     *
     * A return value of `ERR` can be used to specify that the module is unable to handle the device.
     */
    MODULE_EVENT_DEVICE_ATTACH,
    /**
     * @brief Device detach event.
     *
     * This event is sent when a device is detached that the module specified it supports.
     *
     * Return value is ignored.
     */
    MODULE_EVENT_DEVICE_DETACH,
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
            char id[MODULE_MAX_ID]; ///< Device ID string.
        } deviceAttach;
        struct
        {
            char id[MODULE_MAX_ID]; ///< Device ID string.
        } deviceDetach;
    };
} module_event_t;

/**
 * @brief Module procedure and entry point.
 * @typedef module_procedure_t
 */
typedef uint64_t (*module_procedure_t)(module_event_t* event);

/**
 * @brief Module dependency structure.
 * @typedef module_dependency_t
 *
 * Stored in a module's dependency map.
 */
typedef struct module_dependency
{
    map_entry_t entry;
    module_t* module;
} module_dependency_t;

/**
 * @brief Module device structure.
 * @typedef module_device_t
 *
 * Stored in the global device map.
 */
typedef struct module_device
{
    map_entry_t entry;
    module_t* module;       ///< The module handling this device.
    char id[MODULE_MAX_ID]; ///< The devices ID string.
} module_device_t;

/**
 * @brief Module flags.
 * @typedef module_flags_t
 */
typedef enum module_flags
{
    MODULE_FLAG_NONE = 0,
    /**
     * If set, the module was loaded as a dependency, meaning it will be unloaded automatically when no modules depend
     * on it anymore.
     *
     * If this module later gets loaded explicitly, this flag will be cleared.
     *
     * If a module without this flag is explicitly unloaded, but modules depend on it, this flag will be set again.
     */
    MODULE_FLAG_DEPENDENCY = 1 << 0,
    MODULE_FLAG_LOADED = 1 << 1,          ///< If set, the module has received the `MODULE_EVENT_LOAD` event.
    MODULE_FLAG_GC_REACHABLE = 1 << 2,    ///< Used by the GC to mark reachable modules.
    MODULE_FLAG_UNLOADING = 1 << 3,       ///< Prevents re-entrant calls to module_free.
    MODULE_FLAG_HANDLING_DEVICE = 1 << 4, ///< If set, the module is assigned to handle a device.
} module_flags_t;

/**
 * @brief Module structure.
 * @typedef module_t
 */
typedef struct module
{
    map_entry_t dependencyMapEntry; ///< Entry in the global dependency map.
    map_entry_t moduleMapEntry;     ///< Entry in the global module map.
    list_entry_t entry;
    module_flags_t flags;            ///< Module flags, see `module_flags_t`.
    void* baseAddr;                  ///< The address where the modules image is loaded in memory.
    uint64_t size;                   ///< The size of the modules loaded image in memory.
    module_procedure_t procedure;    ///< The module's procedure function and entry point.
    symbol_group_id_t symbolGroupId; ///< See `symbol_group_id_t`.
    map_t dependencies; ///< Map of dependencies, key is `symbol_group_id_t`, value is `module_dependency_t`.
    module_info_t info;
    char* deviceIds; ///< Null-terminated semicolon-separated list of device ID strings.
} module_t;

/*
 * Section for module information.
 */
#define MODULE_INFO_SECTION "._module_info"

/*
 * Section for module device ID table.
 */
#define MODULE_DEVICE_IDS_SECTION "._module_device_ids"

/**
 * @brief Macro to define module information.
 *
 * To define module information we define a separate section in the module's binary called `.module_info` this section
 * stores a concatenated string of the module's name, author, description, version, licence and the OS version, each separated by a `;` and ending with a null-terminator.
 *
 * For example
 * ```c
 * MODULE_INFO("My Module", "John Doe", "A sample module", "1.0.0", "MIT");
 * ```
 * becomes
 * ```c
 * "My Module;John Doe;A sample module;1.0.0;MIT;ac516767\0"
 * ```
 *
 * Should only be used in a module.
 *
 * This section is mandatory.
 *
 * @param _name The name of the module.
 * @param _author The author of the module.
 * @param _description A short description of the module.
 * @param _version The version of the module.
 * @param _licence The licence of the module.
 */
#define MODULE_INFO(_name, _author, _description, _version, _licence) \
    const char _moduleInfo[] __attribute__((section(MODULE_INFO_SECTION), used)) = \
        _name ";" _author ";" _description ";" _version ";" _licence ";" OS_VERSION "\0"

/**
 * @brief Macro to define what device IDs a module supports.
 *
 * This macro is used to specify a set of generic device ID strings that the module supports.
 *
 * These strings can be anything, the module loader does not interpret them in any way, all it does is check for matches
 * when loading modules to handle a specific device ID. For example, this IDs may be ACPI HIDs, PCI IDs, USB IDs or
 * completely custom strings defined by the module itself.
 *
 * To define the device IDs we define a separate section in the module's binary called `.module_devices` this section
 * stores a concatenated string of all device IDs, each separated by a `;` and ending with a null-terminator.
 *
 * For example
 * ```
 * MODULE_DEVICE_IDS("ID1", "ID234", "ID56");
 * ```
 * becomes
 * ```
 * "ID1;ID234;ID56\0"
 * ```
 *
 * Should only be used in a module.
 *
 * This section is optional.
 *
 * @param _string A semicolon-separated list of device ID strings.
 */
#define MODULE_DEVICE_IDS(_string) \
    const char _moduleDeviceIds[] __attribute__((section(MODULE_DEVICE_IDS_SECTION), used)) = _string "\0"

/**
 * @brief The directory where the kernel will look for modules.
 *
 * Note how the OS version is part of the path.
 */
#define MODULE_DIR "/kernel/modules/" OS_VERSION "/:dir"

/**
 * @brief Initialize a fake module representing the kernel itself.
 *
 * Will panic on failure.
 *
 * Used for symbol grouping.
 */
void module_init_fake_kernel_module(const boot_kernel_t* kernel);

/**
 * @brief Load a module from the given path.
 *
 * Will also load any dependencies the module has by checking all other module files located in the same directory as
 * the module being loaded.
 *
 * If a module of the same name is already loaded this function is a no-op and returns success.
 *
 * @param filename The module file name.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t module_load(const char* filename);

/**
 * @brief Unload a module by its name.
 *
 * If the module is currently a dependency this function will fail with `EBUSY`. If the module is not currently
 * considered a dependency but other modules depend on it, it will be demoted to a dependency and not actually unloaded
 * until no modules depend on it anymore.
 *
 * @param name The name of the module to unload, as specified in the `.module_info` section.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t module_unload(const char* name);

/**
 * @brief Load device modules until at least one module is found for each device ID.
 *
 * If a already loaded module supports a device ID, then the ID is considered handled.
 *
 * @param deviceIds Array of device ID strings.
 * @param deviceIdCount Number of device IDs in the array.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t module_load_device_modules(const char** deviceIds, uint64_t deviceIdCount);

/** @} */

#ifdef TESTING
void module_test();
#endif
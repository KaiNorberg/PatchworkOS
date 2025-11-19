#pragma once

#include <_internal/MAX_PATH.h>
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
 * Modules are in effect just ELF binaries which export a `._module_info` which contains metadata about the module.
Check the `MODULE_INFO` macro or more details
 *
 * As expected, each module has an entry point defined by the module linker script as `_module_procedure()`, which can
be thought of as the "main" function of the module but it also does way more than just that, whenever any event occurs
that the module should be aware of this procedure will be called to notify the module of the event.
 *
 * Note that since all global symbols will be exposed to other modules, its a good idea to prefix all global symbols
 * with some unique prefix to avoid naming collisions with other modules, for example `mymodule_*`. The exception to
 * this is symbols starting with '_mod*' which will not be exported or visible to other modules.
 *
 * ## Loading Modules
 *
 * Modules can not be explicitly loaded, instead each module declares what device types it supports in its
`.module_info` section, when the module loader is then told that a device with a specified type is present it will
search for a module supporting that device type and load it. Check the `MODULE_INFO` macro for more details.
 *
 * ## Device Types and Names
 *
 * From the perspective of the module system, devices are identified via a type string and a name string. The type
string, as the name suggests, specifies the type of the device, and there can be multiple devices of the same type.
While the name string must be entirely unique to each instance of a device.
 *
 * As an example, for ACPI, the type string would be the ACPI Hardware ID (HID) of the device, for example "PNP0303" for
a IBM Enhanced PS/2 Keyboard, while the name string would be the full ACPI path to the device in the AML namespace, for
example "\_SB_.PCI0.SF8_.KBD_". But its important to note that the module system does not care or know anything about
the semantics of these strings, it just treats them as opaque strings to identify devices.
 *
 * Since both the type and the name strings are provided to the module during a `MODULE_EVENT_DEVICE_ATTACH` event, the
module is intended to use the name to retrieve more information about the device from the relevant subsystem (for
example ACPI) if needed.
 *
 * ## Dependencies
 *
 * Modules can depend on other modules. For example, module1 could define the function `module_1_func()` and then
 * module2 could call this function. The only way for that to work is for the kernel to load module1 before or during
 * the loading of module2 so that the symbol `module_1_func()` can be resolved when module2 is being relocated.
 *
 * There are many, many ways of handling dependencies. In PatchworkOS it works like this.
 *
 * First, we load some module file, lets say "/kernel/modules/<OS_VERSION>/module2". This module wants to call
`module_1_func()`
 * which is defined in "/kernel/modules/<OS_VERSION>/module1". When resolving the symbols for module2 we will fail to
resolve
 * `module_1_func()`.
 *
 * The failure to resolve a symbol will cause the kernel to search for a module that provides the symbol, it checks all
the symbols in each module eventually finding that module1
 * defines `module_1_func()`. The kernel will then load module1 and retry the symbol resolution for module2, this time
 * succeeding. This repeats until all symbols are resolved or no more modules are found to load.
 *
 * This means that both module1 and module2 need to do exactly nothing, they dont even need to declare that they depend
 * on each other, the kernel will figure it all out automatically.
 *
 * Note that if a module was loaded as a dependency and all modules depending on it are unloaded, the
 * dependency module will also be unloaded, unless it was later explicitly loaded, and if a module was loaded explicitly
 * but later a module depending on it is loaded then it will also wait to be unloaded until all modules depending on it
 * are unloaded.
 *
 * TODO: Currently module symbols and device types are cached in memory after the first load, for now this is fine. But
in the future this cache could become very large so we might need a Linux-style cache file on disk or atleast a way to
invalidate the cache.
 *
 * ## Circular Dependencies
 *
 * When loading a module with dependencies, circular dependencies may occur. For example, module A depends on module B
 * which in turn depends on module A.
 *
 * This is allowed, which means that, for the sake of safety, all modules should be written in such a way that all their
 * global functions can be safely called even if the module is not fully initialized yet. This should rarely make any
difference whatsoever.
 *
 * See "Unloading Modules" below for more details on how circular dependencies are handled during unloading.
 *
 * ## Unloading Modules
 *
 * Modules will be unloaded by the kernel when all the devices they handle are detached and no other loaded module
depends on them.
 *
 * To solve both the issue of dependency tracking and circular dependency resolution, we implement a garbage collector
 * which, using the dependency map, traverses all reachable modules starting from the modules that are currently
handling devices. Any
 * module that is not reachable is considered unused and will be unloaded.
 *
 * @{
 */

/**
 * @brief Sizes for module strings.
 * @typedef module_string_size_t
 */
typedef enum
{
    MODULE_MAX_NAME = 64,
    MODULE_MAX_AUTHOR = 64,
    MODULE_MAX_DESCRIPTION = 256,
    MODULE_MAX_VERSION = 32,
    MODULE_MAX_LICENSE = 64,
    MODULE_MIN_INFO = 6,
    MODULE_MAX_INFO = 1024,
    MODULE_MAX_DEVICE_STRING = 32,
} module_string_size_t;

/**
 * @brief Module information structure.
 * @typedef module_info_t
 *
 * Used to store module information from the `.module_info` section.
 */
typedef struct module_info
{
    char* name;
    char* author;
    char* description;
    char* version;
    char* license;
    char* osVersion;
    char* deviceTypes; ///< Null-terminated semicolon-separated list of device type strings.
    uint64_t dataSize; ///< Size of the `data` field.
    char data[];       ///< All strings are stored here contiguously.
} module_info_t;

/**
 * Section for module information.
 */
#define MODULE_INFO_SECTION "._module_info"

/**
 * @brief Macro to define module information.
 *
 * To define a modules information we use a separate section in the module's binary called `.module_info` this section
 * stores a concatenated string of the module's name, author, description, version, licence, the OS version and the
 * modules device types, each separated by a `;` and ending with a null-terminator.
 *
 * ## Device Types
 *
 * The device types is a semicolon-separated list of generic device type strings that the module supports.
 *
 * These strings can be anything, all the kernel does is check for matches when loading modules to handle a specific
device type and check for the special types listed below. For example, these types may be ACPI HIDs, PCI IDs, USB IDs or
completely custom strings defined by the module itself.
 *
 * Special Device Types:
 * - `LOAD_ON_BOOT`: The module will be loaded after the kernel has initialized itself.
 *
 * ## Data Format
 *
 * As an example of the data format in the `.module_info` section,
 * ```c
 * MODULE_INFO("My Module", "John Doe", "A sample module", "1.0.0", "MIT", "LOAD_ON_BOOT;ACPI0001");
 * ```
 * becomes
 * ```c
 * "My Module;John Doe;A sample module;1.0.0;MIT;ac516767;LOAD_ON_BOOT;ACPI0001\0"
 * ```
 *
 * @param _name The name of the module.
 * @param _author The author of the module.
 * @param _description A short description of the module.
 * @param _version The version of the module.
 * @param _licence The licence of the module.
 * @param _deviceTypes A semicolon-separated list of device type strings that the module supports.
 */
#define MODULE_INFO(_name, _author, _description, _version, _licence, _deviceTypes) \
    const char _moduleInfo[] __attribute__((section(MODULE_INFO_SECTION), used)) = \
        _name ";" _author ";" _description ";" _version ";" _licence ";" OS_VERSION ";" _deviceTypes "\0"

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
 * @brief The directory where the kernel will look for modules.
 *
 * Note how the OS version is part of the path.
 */
#define MODULE_DIR "/kernel/modules/" OS_VERSION "/:dir"

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
     * This event is sent when a device is attached that the module specified it supports.
     *
     * A return value of `ERR` can be used to specify that the module is unable to handle the device.
     */
    MODULE_EVENT_DEVICE_ATTACH,
    /**
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
            char* type;
            char* name;
        } deviceAttach;
        struct
        {
            char* type;
            char* name;
        } deviceDetach;
    };
} module_event_t;

/**
 * @brief Module procedure and entry point.
 * @typedef module_procedure_t
 */
typedef uint64_t (*module_procedure_t)(const module_event_t* event);

/**
 * @brief Module device structure.
 * @typedef module_device_t
 *
 * Represents a device known to the module system to be currently attached.
 */
typedef struct module_device
{
    map_entry_t mapEntry;
    char name[MODULE_MAX_DEVICE_STRING];
    char type[MODULE_MAX_DEVICE_STRING];
    list_t handlers; ///< List of `module_device_handler_t` representing modules handling this device.
} module_device_t;

/**
 * @brief Module device handler structure.
 * @typedef module_device_handler_t
 */
typedef struct module_device_handler
{
    list_entry_t deviceEntry;
    list_entry_t moduleEntry;
    list_entry_t loadEntry;
    module_t* module;
    module_device_t* device;
} module_device_handler_t;

/**
 * @brief Module flags.
 * @typedef module_flags_t
 */
typedef enum module_flags
{
    MODULE_FLAG_NONE = 0,
    MODULE_FLAG_LOADED = 1 << 0,       ///< If set, the module has received the `MODULE_EVENT_LOAD` event.
    MODULE_FLAG_GC_REACHABLE = 1 << 1, ///< Used by the GC to mark reachable modules.
    MODULE_FLAG_GC_PINNED =
        1 << 2, ///< If set, the module will never be collected by the GC, used for the fake kernel module.
} module_flags_t;

/**
 * @brief Module dependency structure.
 * @typedef module_dependency_t
 *
 * We avoid using a map for there as the number of direct dependencies on average should be quite low.
 */
typedef struct
{
    list_entry_t listEntry;
    module_t* module;
} module_dependency_t;

/**
 * @brief Module structure.
 * @typedef module_t
 */
typedef struct module
{
    list_entry_t listEntry;    ///< Entry for the global module list.
    map_entry_t mapEntry;      ///< Entry for the global module map.
    map_entry_t providerEntry; ///< Entry for the module provider map.
    list_entry_t gcEntry;      ///< Entry used for garbage collection.
    list_entry_t loadEntry;    ///< Entry used while loading modules.
    module_flags_t flags;
    void* baseAddr;                  ///< The address where the modules image is loaded in memory.
    uint64_t size;                   ///< The size of the modules loaded image in memory.
    module_procedure_t procedure;    ///< The module's procedure function and entry point.
    symbol_group_id_t symbolGroupId; ///< The symbol group ID for the module's symbols.
    list_t dependencies;             ///< List of `module_dependency_t` representing modules this module depends on.
    list_t deviceHandlers;           ///< List of `module_device_handler_t` representing devices this module handles.
    module_info_t info;
} module_t;

/**
 * @brief Module symbol cache entry structure.
 * @struct module_cached_symbol_t
 */
typedef struct
{
    map_entry_t mapEntry;
    char* modulePath; ///< Path to the module defining the symbol.
} module_cached_symbol_t;

/**
 * @brief Module device cache entry structure.
 * @struct module_cached_device_entry_t
 */
typedef struct
{
    list_entry_t listEntry;
    char path[MAX_PATH]; ///< Path to the module supporting the device.
} module_cached_device_entry_t;

/**
 * @brief Module device cache entry structure.
 * @struct module_cached_device_t
 */
typedef struct
{
    map_entry_t mapEntry;
    list_t entries; ///< List of `module_cached_device_entry_t`.
} module_cached_device_t;

/**
 * @brief Module load flags.
 * @typedef module_load_flags_t
 */
typedef enum
{
    MODULE_LOAD_ONE = 0 << 0, ///< If set, will load only the first module matching the device type.
    MODULE_LOAD_ALL = 1 << 0, ///< If set, will load all modules matching the device type.
} module_load_flags_t;

/**
 * @brief Initialize a fake module representing the kernel itself.
 *
 * Will panic on failure.
 *
 * Used for symbol grouping.
 */
void module_init_fake_kernel_module(const boot_kernel_t* kernel);

/**
 * @brief Notify the module system of a device being attached.
 *
 * Will automatically load any dependencies required by the module.
 *
 * If a module fails to load, we do not consider it a fatal error, instead we log the error and continue loading other
 * modules.
 *
 * @param type The device type string.
 * @param name The unique device name string.
 * @param flags Load flags, see `module_load_flags_t`.
 * @return On success, the amount of modules loaded. On failure, `ERR` and `errno` is set.
 */
uint64_t module_device_attach(const char* type, const char* name, module_load_flags_t flags);

/**
 * @brief Notify the module system of a device being detached.
 *
 * If a module to unload is not currently considered a dependency but other modules depend on it, it will be demoted to
 * a dependency and not actually unloaded until no modules depend on it anymore.
 *
 * @param name The unique device name string, or `NULL` for no-op.
 */
void module_device_detach(const char* name);

/** @} */
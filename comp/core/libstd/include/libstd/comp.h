#ifndef _SYS_COMP_H
#define _SYS_COMP_H 1

#include <libstd/io.h>
#include <libstd/scon.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Component Management
 * @defgroup libstd_comp Component Management
 * @ingroup libstd
 *
 * The `sys/comp.h` header provides definitions for managing user-space components.
 *
 * All components is stored as a directory in the `/comp` directory, with the complete path to a component being
 * described as `/comp/<name>/<x>.<y>.<z>`.
 *
 * ## Manifests
 *
 * Each components version directories contain a "manifest" file, this file stores the configuration for the component,
 * such as its dependencies, capabilities, etc.
 *
 * @see libstd_scon for more information.
 *
 * ### Expressions
 *
 * Included is a list of expressions that the manifest file will understand:
 *
 * - `(component ...)` The root expression of the manifest.
 * - `(description "...")` A short description of the component.
 * - `(author ...)` The author of the component.
 * - `(license ...)` The license of the component.
 * - `(launch ...)` The path to the executable to run when the component is launched, only needed for launchable.
 * components.
 * - `(module ...)` The path(s) to the kernel module(s) that the component provides, this and launch cannot be specified
 * together. Only needed for components that provide modules.
 * - `(dependencies ...)` A list of expressions in the format `(name major.minor.patch)` describing components that this
 * component depends on with the version being the minimum version required.
 * - `(capabilities ...)` A list of paths that the component requires to run.
 *
 * ### Capabilities
 *
 * A capability is a filesystem path to some resource that a component requires to run.
 *
 * Example paths:
 * - `/dev/fb` Access to the framebuffer directory.
 * - `/dev/kbd` Access to the keyboard directory.
 * - `/sys/fs` Access to the filesystem directory.
 *
 * ### Bindings
 *
 * The following directories will be, if found within a component, automatically bound to the component's root
 * directory:
 *
 * - `/bin` The component's executable directory.
 * - `/lib` The component's library directory.
 * - `/include` The component's C/C++ header files.
 * - `/data` The component's static assets and read-only data.
 * - `/cfg` The component's default configuration files.
 *
 *
 * ### Minimum Version Selection
 *
 * The component system uses Minimum Version Selection, will always choose the lowest possible version of components
 * that satisfies all dependencies.
 *
 * This ensures that the system is reproducible and that any updates are explicit as the same set of dependencies will
 * always result in the same environment, given the same manifests.
 *
 * We also avoid NP-complete version selection problems which is good for my sanity.
 *
 * @{
 */

/**
 * @brief Component version structure.
 * @struct comp_version_t
 */
typedef struct
{
    union {
        struct
        {
            uint32_t major;
            uint32_t minor;
            uint32_t patch;
        };
        uint32_t array[3];
    };
} comp_version_t;

/**
 * @brief Component dependency structure.
 * @struct comp_dependency_t
 */
typedef struct
{
    char name[MAX_NAME];
    comp_version_t version;
    scon_t scon;
    char* manifest;
    size_t manifestLength;
} comp_dependency_t;

/**
 * @brief Options for launching a component.
 * @struct comp_options_t
 */
typedef struct comp_options
{
    fd_t stdin;             ///< Standard input file descriptor.
    fd_t stdout;            ///< Standard output file descriptor.
    fd_t stderr;            ///< Standard error file descriptor.
    uint8_t _reserved[482]; ///< Reserved for future use.
} comp_options_t;

#ifdef static_assert
static_assert(sizeof(comp_options_t) == 512, "comp_options_t must be 512 bytes");
#endif

/**
 * @brief Launch a component with its declared launch executable.
 *
 * If the specified component does not specify `(launch ...)` or if any dependency specifies a kernel module, this
 * function will fail.
 *
 * @param name The name of the component to launch.
 * @param version The minimum version to launch.
 * @param opts Additional launch options, or `NULL` for defaults.
 * @return An appropriate status value.
 */
status_t comp_launch(const char* name, const char* version, const comp_options_t* opts);

/**
 * @brief Find all dependencies for a given component.
 *
 * The output array must be freed using `comp_dependencies_free()`.
 *
 * The output array will also contain the component itself.
 *
 * @param name The name of the component.
 * @param version The minimum version of the component.
 * @param outDeps Output pointer for the dynamically allocated array of dependencies.
 * @param outCount Output pointer for the number of dependencies found.
 * @return An appropriate status value.
 */
status_t comp_dependencies_get(const char* name, const char* version, comp_dependency_t** outDeps, size_t* outCount);

/**
 * @brief Free an array of component dependencies.
 *
 * @param deps The array of dependencies to free.
 * @param count The number of dependencies in the array.
 */
void comp_dependencies_free(comp_dependency_t* deps, size_t count);

/** @} */

#endif

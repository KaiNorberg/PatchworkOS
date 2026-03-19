#ifndef _SYS_COMP_H
#define _SYS_COMP_H 1

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/io.h>

/**
 * @brief Component Management
 * @defgroup libstd_sys_comp Component Management
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
 * @see libstd_sys_scon for more information.
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
 * - `(dependencies ...)` A list of expressions in the format `(name [operator] version) describing components that this
 * component depends on.
 * - `(capabilities ...) A list of capabilities that the component requires to run.
 *
 * The operator within the `(dependencies ...)` expression can be one of the following:
 * - `==` Exactly the specified version.
 * - `!=` Not the specified version.
 * - `>` Greater than the specified version.
 * - `<` Less than the specified version.
 * - `>=` Greater than or equal to the specified version.
 * - `<=` Less than or equal to the specified version.
 *
 * The `?` prefix can be applied to any operator to indicate that the dependency is optional. For example, `(libstd
 * ?>= 1.0.0)`.
 *
 * ### Capabilities
 *
 * A capability is a string that describes a resource or permission that the component requires to run, included below
 * is a list of capabilities that the manifest file will understand:
 *
 * - `comp` Access to everything required to launch and manage other components.
 * - `fb` Access to the `/dev/fb` directory.
 * - `kbd` Access to the `/dev/kbd` directory.
 * - `mouse` Access to the `/dev/mouse` directory.
 *
 * @todo Document more capabilities.
 *
 * @{
 */

/**
 * @brief Options for launching a component.
 * @struct comp_launch_opts_t
 */
typedef struct comp_launch_opts
{
    const char* version;    ///< The version requirement (e.g., ">= 1.0.0"). If `NULL`, the latest version is chosen.
    uint8_t _reserved[504]; ///< Reserved for future use.
} comp_launch_opts_t;

#ifdef static_assert
static_assert(sizeof(comp_launch_opts_t) == 512, "comp_launch_opts_t must be 512 bytes");
#endif

/**
 * @brief Launch a component with its declared launch executable.
 *
 * If the specified component does not specify `(launch ...)` this function will fail.
 *
 * @todo Implement `comp_launch()`.
 *
 * @param name The name of the component to launch.
 * @param opts Additional launch options, or `NULL` for defaults.
 * @return An appropriate status value.
 */
status_t comp_launch(const char* name, const comp_launch_opts_t* opts);

/** @} */

#endif
#pragma once

#include <stdint.h>

/**
 * @brief Package Manifest Files.
 * @defgroup programs_pkgd_manifest Manifests.
 * @ingroup programs_pkgd
 *
 * All packages must include a manifest file located at `/pkg/<package>/manifest` using the below format.
 *
 * ## Format
 *
 * ```
 * [meta]
 * description = <short description of the package>
 * version = <version string>
 * author = <author name>
 * license = <license>
 *
 * [exec]
 * bin = <path to the main executable, specified in the packages namespace>
 * priority = <scheduler priority [`PRIORITY_MIN`, `PRIORITY_MAX_USER`]>
 *
 * [sandbox]
 * profile = <empty|copy|share|inherit>
 * foreground = <true|false>
 *
 * [env]
 * KEY = VALUE ; Environment variable key-value pairs.
 * ...
 *
 * [namespace]
 * <target> = <source> ; Flags should be specified with the target, the source is specified in the pkgd's namespace.
 * ```
 *
 * ## Sandbox Profiles
 *
 * There are four possible sandbox profiles:
 * - `empty`: Start with an empty namespace, meaning the process will by default not have access to any files or
 * devices.
 * - `inherit`: Inherit the caller's namespace. This is useful for system utilities like `ls` or `grep` that need to
 * operate on the user's current environment.
 *
 * @todo Implement the inherit sandbox profile.
 *
 * ## Foreground Mode
 *
 * If `foreground` is set to `true`, then the package will receive stdio from the creator, be in the same process-group
 * as the creator and start with the same cwd as the creator. Finally, the creator will receive a key to the packages
 * `/proc/[pid]/wait` file to retrieve its exit status.
 *
 * In short, in foreground mode the package will, as far as the creator is concerned, behave like a child process.
 *
 * @todo Implement foreground mode.
 *
 * @{
 */

#define MANIFEST_STRING_MAX 128

#define MANIFEST_SECTION_MAX 64

typedef struct
{
    char key[MANIFEST_STRING_MAX];
    char value[MANIFEST_STRING_MAX];
} section_entry_t;

typedef struct
{
    section_entry_t entries[MANIFEST_SECTION_MAX];
    uint64_t amount;
} section_t;

typedef enum
{
    SECTION_META,
    SECTION_EXEC,
    SECTION_SANDBOX,
    SECTION_ENV,
    SECTION_NAMESPACE,
    SECTION_TYPE_MAX,
} section_type_t;

typedef struct
{
    section_t sections[SECTION_TYPE_MAX];
} manifest_t;

uint64_t manifest_parse(const char* path, manifest_t* manifest);

typedef struct
{
    char* key;
    char* value;
} substitution_t;

void manifest_substitute(manifest_t* manifest, substitution_t* substitutions, uint64_t amount);

char* manifest_get_value(section_t* section, const char* key);

uint64_t manifest_get_integer(section_t* section, const char* key);

/** @} */
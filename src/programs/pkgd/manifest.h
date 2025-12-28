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
 * [env]
 * KEY = VALUE
 * 
 * [sandbox]
 * profile = <empty|copy|share>
 *
 * [namespace]
 * <source, with flags> = <target>
 * ```
 * 
 * ## Sandbox Profiles
 * 
 * There are three possible sandbox profiles:
 * - `empty`: Start with an empty namespace, meaning the process will by default not have access to any files or devices.
 * - `copy`: Copy the pkgd's namespace, meaning the process will have total access to the same files and devices as the pkgd but changes to the namespace will not affect the pkgd.
 * - `share`: Share the pkgd's namespace, meaning any changes to the namespace will affect both the pkgd and the process.
 * 
 * @warning The copy and share profiles should only be used for trusted packages as they provide almost complete access to the system.
 * 
 * @todo Implement sandbox profiles.
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
    SECTION_ENV,
    SECTION_SANDBOX,
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
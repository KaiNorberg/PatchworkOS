#pragma once

#include <stdint.h>

/**
 * @brief Package Manifest Files.
 * @defgroup programs_pkgd_manifest Manifests.
 * @ingroup programs_pkgd
 * 
 * All packages must include a manifest file located at `/pkg/<package>/manifest` using the below format.
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
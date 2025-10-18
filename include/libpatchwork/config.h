#ifndef CONFIG_CONFIG_H
#define CONFIG_CONFIG_H 1

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @brief System configuration files.
 * @defgroup libpatchwork_config Config files.
 * @ingroup libpatchwork
 *
 * Patchwork uses a `/cfg` folder for all system configuration files. These files are simple INI style text files that store
 * key-value pairs in sections.
 *
 * @{
 */

typedef struct config config_t;

typedef struct config_array config_array_t;

config_t* config_open(const char* prefix, const char* name);

void config_close(config_t* cfg);

uint64_t config_scanf(config_t* cfg, const char* section, const char* key, const char* format, ...);

const char* config_get_string(config_t* cfg, const char* section, const char* key, const char* fallback);

int64_t config_get_int(config_t* cfg, const char* section, const char* key, int64_t fallback);

bool config_get_bool(config_t* cfg, const char* section, const char* key, bool fallback);

config_array_t* config_get_array(config_t* cfg, const char* section, const char* key);

void config_array_free(config_array_t* array);

uint64_t config_array_length(config_array_t* array);

const char* config_array_get_string(config_array_t* array, uint64_t index, const char* fallback);

int64_t config_array_get_int(config_array_t* array, uint64_t index, int64_t fallback);

bool config_array_get_bool(config_array_t* array, uint64_t index, bool fallback);

/** @} */

#if defined(__cplusplus)
}
#endif

#endif

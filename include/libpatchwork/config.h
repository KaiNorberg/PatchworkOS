#ifndef CONFIG_CONFIG_H
#define CONFIG_CONFIG_H 1

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

// Note: Config files always fail safely (apart from arrays and scanf but shhh), the idea being that an attempt to read
// from a config file that failed to open will always return the fallback values.

typedef struct config config_t;

typedef struct config_array config_array_t;

config_t* config_open(const char* prefix, const char* name);

void config_close(config_t* cfg);

uint64_t config_scanf(config_t* cfg, const char* section, const char* key, const char* format, ...);

const char* config_string_get(config_t* cfg, const char* section, const char* key, const char* fallback);

int64_t config_int_get(config_t* cfg, const char* section, const char* key, int64_t fallback);

bool config_bool_get(config_t* cfg, const char* section, const char* key, bool fallback);

config_array_t* config_array_get(config_t* cfg, const char* section, const char* key);

void config_array_free(config_array_t* array);

uint64_t config_array_length(config_array_t* array);

const char* config_array_string_get(config_array_t* array, uint64_t index, const char* fallback);

int64_t config_array_int_get(config_array_t* array, uint64_t index, int64_t fallback);

bool config_array_bool_get(config_array_t* array, uint64_t index, bool fallback);

#if defined(__cplusplus)
}
#endif

#endif

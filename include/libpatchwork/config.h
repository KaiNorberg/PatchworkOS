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
 * @defgroup libpatchwork_config Config files
 * @ingroup libpatchwork
 *
 * Patchwork uses a `/cfg` folder for all system configuration files. These files are simple INI style text files that
 * store key-value pairs in sections.
 *
 * TODO: The current system is rather simplistic and in the future it might, if i can be bothered, be worth implementing
 * a database like configuration system.
 *
 * @{
 */

/**
 * @brief Opaque configuration structure.
 * @struct config_t
 */
typedef struct config config_t;

/**
 * @brief Configuration array structure.
 * @struct config_array_t
 */
typedef struct config_array
{
    char** items;
    uint64_t length;
} config_array_t;

/**
 * @brief Open a configuration file.
 *
 * All configuration files have this full path: `/cfg/<prefix>-<name>.cfg`.
 *
 * The goal is that each system or application uses its own prefix to avoid name collisions.
 *
 * @param prefix The prefix of the configuration file, for example `theme` for theme related settings.
 * @param name The name of the configuration file, for example `colors` for color related settings.
 * @return On success, the opened configuration file. On failure, returns `NULL` and `errno` is set.
 */
config_t* config_open(const char* prefix, const char* name);

/**
 * @brief Close a configuration file.
 *
 * @param config The configuration file to close.
 */
void config_close(config_t* config);

/**
 * @brief Get a string value from a configuration file.
 *
 * @param config The configuration file.
 * @param section The section to get the value from, case insensitive.
 * @param key The key to get the value for, case insensitive.
 * @param fallback A default value to return if the key is not found.
 * @return The string value if found, or `fallback` otherwise.
 */
const char* config_get_string(config_t* config, const char* section, const char* key, const char* fallback);

/**
 * @brief Get an integer value from a configuration file.
 *
 * @param config The configuration file.
 * @param section The section to get the value from, case insensitive.
 * @param key The key to get the value for, case insensitive.
 * @param fallback A default value to return if the key is not found or cannot be parsed as an integer.
 * @return The integer value if found and parsed, or `fallback` otherwise.
 */
int64_t config_get_int(config_t* config, const char* section, const char* key, int64_t fallback);

/**
 * @brief Get a boolean value from a configuration file.
 *
 * Recognized "true" values (case-insensitive): "true", "yes", "on", "1".
 * Recognized "false" values (case-insensitive): "false", "no", "off", "0".
 *
 * @param config The configuration file.
 * @param section The section to get the value from, case insensitive.
 * @param key The key to get the value for, case insensitive.
 * @param fallback A default value to return if not found or unrecognized.
 * @return The boolean value if found and parsed, or `fallback` otherwise.
 */
bool config_get_bool(config_t* config, const char* section, const char* key, bool fallback);

/**
 * @brief Get an array of strings from a configuration file.
 *
 * Parses the string value into an array of strings, split by commas and with the whitespace trimmed.
 *
 * @param config The configuration file.
 * @param section The section to get the value from, case insensitive.
 * @param key The key to get the value for, case insensitive.
 * @return The configuration array or an empty array.
 */
config_array_t* config_get_array(config_t* config, const char* section, const char* key);

/**
 * @brief Free a configuration array.
 *
 * @param array The configuration array to free.
 */
void config_array_free(config_array_t* array);

/** @} */

#if defined(__cplusplus)
}
#endif

#endif

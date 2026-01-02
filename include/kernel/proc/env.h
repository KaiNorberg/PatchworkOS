#pragma once

#include <kernel/sync/mutex.h>

#include <stdlib.h>
#include <sys/defs.h>

/**
 * @brief Environment variables.
 * @defgroup kernel_proc_env Environment
 * @ingroup kernel_proc
 *
 * @{
 */

/**
 * @brief Environment variable structure.
 * @struct env_var_t
 */
typedef struct
{
    char* key;
    char* value;
} env_var_t;

/**
 * @brief Environment structure.
 * @struct env_t
 *
 * Stored in `process_t::env`.
 */
typedef struct
{
    env_var_t* vars;
    size_t count;
    mutex_t mutex;
} env_t;

/**
 * @brief Initialize the environment.
 *
 * @param env The environment to initialize.
 */
void env_init(env_t* env);

/**
 * @brief Deinitialize the environment.
 *
 * @param env The environment to deinitialize.
 */
void env_deinit(env_t* env);

/**
 * @brief Copy environment variables from one environment to another.
 *
 * @param dest The destination environment.
 * @param src The source environment.
 * @return On success, `0`. On failure, returns `ERR` and `errno` is set.
 */
uint64_t env_copy(env_t* dest, env_t* src);

/**
 * @brief Get the value of an environment variable.
 *
 * @param env The environment to search in.
 * @param key The name of the environment variable.
 * @return A pointer to the value of the environment variable or `NULL` if it does not exist.
 */
const char* env_get(env_t* env, const char* key);

/**
 * @brief Set the value of an environment variable.
 *
 * If the variable already exists, its value will be updated.
 *
 * @param env The environment to modify.
 * @param key The name of the environment variable.
 * @param value The value to set the environment variable to.
 * @return On success, `0`. On failure, returns `ERR` and `errno` is set.
 */
uint64_t env_set(env_t* env, const char* key, const char* value);

/**
 * @brief Unset an environment variable.
 *
 * If the variable does not exist, this function does nothing.
 *
 * @param env The environment to modify.
 * @param key The name of the environment variable.
 * @return On success, `0`. On failure, returns `ERR` and `errno` is set.
 */
uint64_t env_unset(env_t* env, const char* key);

/** @} */

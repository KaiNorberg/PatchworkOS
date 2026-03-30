#pragma once

#include <libc/status.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Dynamic string.
 * @defgroup libc_dstr dstr
 * @ingroup libc
 *
 * A dynamic heap-allocated string with small object optimization.
 *
 * @{
 */

#define DSTR_SMALL_MAX 40 ///< The size used for small buffer optimization in the `dstr_t` structure.

/**
 * @brief Dynamic string structure.
 * @struct dstr_t
 */
typedef struct
{
    char* data;
    size_t length;
    size_t capacity;
    char small[DSTR_SMALL_MAX];
} dstr_t;

/**
 * @brief Initialize a dynamic string.
 *
 * @param dstr Pointer to the dynamic string structure to initialize.
 * @param data The initial string data.
 * @param length The length of the initial string data, can be `0`.
 * @return An appropriate status value.
 */
static status_t dstr_init(dstr_t* dstr, const char* data, size_t length)
{
    if (length + 1 <= DSTR_SMALL_MAX)
    {
        dstr->data = dstr->small;
        dstr->length = length;
        dstr->capacity = length;
        memcpy(dstr->data, data, length);
        dstr->data[length] = '\0';
        return OK;
    }

    dstr->data = malloc(length + 1);
    if (dstr->data == NULL)
    {
        return ERR(LIBSTD, NOMEM);
    }
    dstr->data[length] = '\0';
    memcpy(dstr->data, data, length);
    dstr->length = length;
    dstr->capacity = length;

    return OK;
}

/**
 * @brief Deinitialize a dynamic string.
 *
 * @param dstr Pointer to the dynamic string structure to deinitialize.
 */
static void dstr_deinit(dstr_t* dstr)
{
    if (dstr->data != dstr->small)
    {
        free(dstr->data);
    }
    dstr->data = NULL;
    dstr->length = 0;
    dstr->capacity = 0;
}

/**
 * @brief Append a string to a dynamic string.
 *
 * @param dstr Pointer to the dynamic string structure.
 * @param data The string data to append.
 * @param length The length of the string data to append.
 * @return An appropriate status value.
 */
status_t dstr_append(dstr_t* dstr, const char* data, size_t length);

/**
 * @brief Clear the dynamic string.
 *
 * @param dstr Pointer to the dynamic string structure.
 */
static inline void dstr_clear(dstr_t* dstr)
{
    dstr->length = 0;
    if (dstr->data != NULL)
    {
        dstr->data[0] = '\0';
    }
}

/**
 * @brief Set the data within a dynamic string.
 *
 * @param dstr Pointer to the dynamic string structure.
 * @param data The string data to set.
 * @param length The length of the string data to set.
 * @return An appropriate status value.
 */
static inline status_t dstr_set(dstr_t* dstr, const char* data, size_t length)
{
    dstr_clear(dstr);
    return dstr_append(dstr, data, length);
}

/**
 * @brief Compare a dynamic string with a string buffer.
 *
 * @param dstr Pointer to the dynamic string structure.
 * @param str The string to compare against.
 * @return `true` if the strings are equal, `false` otherwise.
 */
static inline bool dstr_eq(const dstr_t* dstr, const char* str, size_t length)
{
    if (dstr->length != length)
    {
        return false;
    }
    return strncmp(dstr->data, str, length) == 0;
}

/** @} */
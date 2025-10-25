#pragma once

#include "file.h"
#include "utils/map.h"

#include <sys/io.h>
#include <sys/list.h>

/**
 * @brief Keys for sharing file descriptors between processes.
 * @defgroup kernel_fs_key Keys
 * @ingroup kernel_fs
 *
 * Keys are used with the `share()` and `claim()` system calls to send files between processes.
 *
 * Each key is a 64-bit one-time use randomly generated token that globally identifies a shared file.
 *
 * @{
 */

/**
 * @brief Key entry.
 */
typedef struct
{
    list_entry_t entry; ///< Used to store the key entry in a time sorted list.
    map_entry_t mapEntry; ///< Used to store the key entry in a map for fast lookup.
    key_t key;
    file_t* file;
    clock_t expiry;
} key_entry_t;

/**
 * @brief Initializes the key subsystem.
 */
void key_init(void);

/**
 * @brief Generates a key that can be used to retrieve the file within the specified timeout.
 *
 * @param key Output pointer to store the generated key.
 * @param file The file to share.
 * @param timeout The time until the shared file expires. If `CLOCKS_NEVER`, it never expires.
 * @return On success, a key that can be used to claim the file. On failure, `ERR` and `errno` is set.
 */
uint64_t key_share(key_t* key, file_t* file, clock_t timeout);

/**
 * @brief Claims a shared file using the provided key.
 *
 * @param key Pointer to the key identifying the shared file.
 * @return On success, the claimed file. On failure, `NULL` and `errno` is set.
 */
file_t* key_claim(key_t* key);

/** @} */

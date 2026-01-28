#pragma once

#include <kernel/fs/file.h>

#include <sys/fs.h>
#include <sys/list.h>
#include <sys/map.h>

/**
 * @brief Keys for sharing file descriptors between processes.
 * @defgroup kernel_fs_key Keys
 * @ingroup kernel_fs
 *
 * Keys are used with the `share()` and `claim()` system calls to send files between processes.
 *
 * Each key is a one-time use randomly generated base64URL encoded string that globally identifies a shared file.
 *
 * @see https://en.wikipedia.org/wiki/Base64
 *
 * @{
 */

/**
 * @brief Key entry.
 */
typedef struct
{
    list_entry_t entry;
    map_entry_t mapEntry;
    char key[KEY_MAX];
    file_t* file;
    clock_t expiry;
} key_entry_t;

/**
 * @brief Generates a key that can be used to retrieve the file within the specified timeout.
 *
 * @param key Output buffer to store the generated key.
 * @param size The size of the output buffer.
 * @param file The file to share.
 * @param timeout The time until the shared file expires. If `CLOCKS_NEVER`, it never expires.
 * @return An appropriate status value.
 */
status_t key_share(char* key, uint64_t size, file_t* file, clock_t timeout);

/**
 * @brief Claims a shared file using the provided key.
 *
 * @param out Output pointer to store the claimed file.
 * @param key The key identifying the shared file.
 * @return An appropriate status value.
 */
status_t key_claim(file_t** out, const char* key);

/** @} */

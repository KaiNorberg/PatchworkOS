#pragma once

#include <kernel/mem/paging_types.h>

#include <stddef.h>
#include <stdint.h>
#include <sys/status.h>

/**
 * @brief Page Vector structure.
 * @defgroup Page Vector
 * @ingroup kernel_mem_pagevec
 *
 * A Page Vector is intended as a lightweight alternative to the Scatter Gather List structure, primarily used for
 * backing shared memory buffers.
 *
 * @{
 */

/**
 * @brief Structure representing a dynamically sized array of physical pages.
 * @struct pagevec_t
 */
typedef struct pagevec
{
    pfn_t* pfns;
    size_t amount;
    size_t capacity;
} pagevec_t;

/**
 * @brief Initializes a page vector.
 *
 * @param vec The vector to initialize.
 */
static inline void pagevec_init(pagevec_t* vec)
{
    vec->pfns = NULL;
    vec->amount = 0;
    vec->capacity = 0;
}

/**
 * @brief Deinitializes a page vector, freeing all of its physical pages and memory.
 *
 * @param vec The vector to deinitialize.
 */
void pagevec_deinit(pagevec_t* vec);

/**
 * @brief Resizes a page vector, allocating or freeing physical pages as necessary.
 *
 * Newly allocated pages will be zeroed.
 *
 * @param vec The vector to resize.
 * @param pageAmount The new amount of pages.
 * @return An appropriate status value.
 */
status_t pagevec_resize(pagevec_t* vec, size_t pageAmount);

/**
 * @brief Writes data from a buffer into a page vector.
 *
 * @param vec The vector to write to.
 * @param offset The byte offset to start writing at.
 * @param buffer The buffer to read from.
 * @param length The amount of bytes to write.
 */
void pagevec_write(pagevec_t* vec, size_t offset, const void* buffer, size_t length);

/**
 * @brief Reads data from a page vector into a buffer.
 *
 * @param vec The vector to read from.
 * @param offset The byte offset to start reading from.
 * @param buffer The buffer to write to.
 * @param length The amount of bytes to read.
 */
void pagevec_read(const pagevec_t* vec, size_t offset, void* buffer, size_t length);

/**
 * @brief Fills a region of a page vector with a specific byte value.
 *
 * @param vec The vector to fill.
 * @param offset The byte offset to start filling at.
 * @param value The value to fill with.
 * @param length The amount of bytes to fill.
 */
void pagevec_fill(pagevec_t* vec, size_t offset, uint8_t value, size_t length);

/** @} */
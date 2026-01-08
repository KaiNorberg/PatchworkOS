#pragma once

#include <errno.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief First-in first-out buffer.
 * @defgroup kernel_utils_fifo FIFO Buffer
 * @ingroup kernel_utils
 *
 * @{
 */

/**
 * @brief FIFO Buffer.
 * @struct fifo_t
 */
typedef struct fifo
{
    uint8_t* buffer; ///< Pointer to the buffer memory.
    size_t size;     ///< The total size of the buffer.
    size_t head;     ///< The position to write to.
    size_t tail;     ///< The position to start reading from.
} fifo_t;

/**
 * @brief Create a fifo buffer initializer.
 *
 * @param _buf Pointer to the buffer memory.
 * @param _size The size of the buffer in bytes.
 */
#define FIFO_CREATE(_buf, _size) {.buffer = (uint8_t*)(_buf), .size = (_size), .head = 0, .tail = 0}

/**
 * @brief Define and initialize a fifo buffer.
 *
 * Helps define a fifo buffer with a backing buffer.
 *
 * @param _name The name of the fifo buffer.
 * @param _size The size of the fifo buffer in bytes.
 */
#define FIFO_DEFINE(_name, _size) \
    uint8_t _name##_buffer[_size]; \
    fifo_t _name = FIFO_CREATE(_name##_buffer, _size)

/**
 * @brief Initialize a fifo buffer.
 *
 * @param fifo Pointer to the fifo buffer structure.
 * @param buffer Pointer to the buffer memory.
 * @param size The size of the buffer in bytes.
 */
void fifo_init(fifo_t* fifo, uint8_t* buffer, size_t size);

/**
 * @brief Reset a fifo buffer.
 *
 * @param fifo Pointer to the fifo buffer structure.
 */
void fifo_reset(fifo_t* fifo);

/**
 * @brief Return the number of bytes available for reading in a fifo buffer.
 *
 * @param fifo Pointer to the fifo buffer structure.
 * @return The number of bytes used.
 */
size_t fifo_bytes_readable(const fifo_t* fifo);

/**
 * @brief Return the number of bytes available for writing in a fifo buffer.
 *
 * @param fifo Pointer to the fifo buffer structure.
 * @return The number of bytes available for writing.
 */
size_t fifo_bytes_writeable(const fifo_t* fifo);

/**
 * @brief Read data from a fifo buffer at a specific offset.
 *
 * @param fifo The fifo buffer structure.
 * @param buffer The destination buffer.
 * @param count The number of bytes to read.
 * @return The number of bytes read.
 */
size_t fifo_read(fifo_t* fifo, void* buffer, size_t count);

/**
 * @brief Write data to the fifo buffer.
 *
 * Will write up to the available space.
 *
 * @param fifo Pointer to the fifo buffer structure.
 * @param buffer The source buffer.
 * @param count The number of bytes to write.
 * @return The number of bytes written.
 */
size_t fifo_write(fifo_t* fifo, const void* buffer, size_t count);

/**
 * @brief Advance the head of the fifo buffer.
 *
 * @param fifo Pointer to the fifo buffer structure.
 * @param count The number of bytes to advance the head by.
 */
void fifo_advance_head(fifo_t* fifo, size_t count);

/**
 * @brief Advance the tail of the fifo buffer.
 *
 * @param fifo Pointer to the fifo buffer structure.
 * @param count The number of bytes to advance the tail by.
 */
void fifo_advance_tail(fifo_t* fifo, size_t count);

/** @} */

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
static inline void fifo_init(fifo_t* fifo, uint8_t* buffer, size_t size)
{
    fifo->buffer = buffer;
    fifo->size = size;
    fifo->head = 0;
    fifo->tail = 0;
}

/**
 * @brief Reset a fifo buffer.
 *
 * @param fifo Pointer to the fifo buffer structure.
 */
static inline void fifo_reset(fifo_t* fifo)
{
    fifo->head = 0;
    fifo->tail = 0;
}

/**
 * @brief Return the number of bytes available for reading in a fifo buffer.
 *
 * @param fifo Pointer to the fifo buffer structure.
 * @return The number of bytes used.
 */
static inline size_t fifo_bytes_readable(const fifo_t* fifo)
{
    if (fifo->head >= fifo->tail)
    {
        return fifo->head - fifo->tail;
    }

    return fifo->size - (fifo->tail - fifo->head);
}

/**
 * @brief Return the number of bytes available for writing in a fifo buffer.
 *
 * @param fifo Pointer to the fifo buffer structure.
 * @return The number of bytes available for writing.
 */
static inline size_t fifo_bytes_writeable(const fifo_t* fifo)
{
    if (fifo->tail > fifo->head)
    {
        return fifo->tail - fifo->head - 1;
    }

    return fifo->size - (fifo->head - fifo->tail) - 1;
}

/**
 * @brief Read data from a fifo buffer at a specific offset.
 *
 * @param fifo The fifo buffer structure.
 * @param buffer The destination buffer.
 * @param count The number of bytes to read.
 * @return The number of bytes read.
 */
static inline size_t fifo_read(fifo_t* fifo, void* buffer, size_t count)
{
    size_t readable = fifo_bytes_readable(fifo);
    if (readable == 0)
    {
        return 0;
    }

    if (count > readable)
    {
        count = readable;
    }

    size_t firstSize = fifo->size - fifo->tail;
    if (firstSize > count)
    {
        firstSize = count;
    }

    memcpy(buffer, fifo->buffer + fifo->tail, firstSize);
    fifo->tail = (fifo->tail + firstSize) % fifo->size;

    size_t remaining = count - firstSize;
    if (remaining > 0)
    {
        memcpy((uint8_t*)buffer + firstSize, fifo->buffer + fifo->tail, remaining);
        fifo->tail = (fifo->tail + remaining) % fifo->size;
    }

    return count;
}

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
static inline size_t fifo_write(fifo_t* fifo, const void* buffer, size_t count)
{
    size_t writeable = fifo_bytes_writeable(fifo);
    if (count > writeable)
    {
        count = writeable;
    }

    size_t firstSize = fifo->size - fifo->head;
    if (firstSize > count)
    {
        firstSize = count;
    }

    memcpy(fifo->buffer + fifo->head, buffer, firstSize);
    fifo->head = (fifo->head + firstSize) % fifo->size;

    size_t remaining = count - firstSize;
    if (remaining > 0)
    {
        memcpy(fifo->buffer + fifo->head, (uint8_t*)buffer + firstSize, remaining);
        fifo->head = (fifo->head + remaining) % fifo->size;
    }

    return count;
}

/**
 * @brief Advance the head of the fifo buffer.
 *
 * @param fifo Pointer to the fifo buffer structure.
 * @param count The number of bytes to advance the head by.
 */
static inline void fifo_advance_head(fifo_t* fifo, size_t count)
{
    fifo->head = (fifo->head + count) % fifo->size;
}

/**
 * @brief Advance the tail of the fifo buffer.
 *
 * @param fifo Pointer to the fifo buffer structure.
 * @param count The number of bytes to advance the tail by.
 */
static inline void fifo_advance_tail(fifo_t* fifo, size_t count)
{
    fifo->tail = (fifo->tail + count) % fifo->size;
}

/** @} */

#pragma once

#include <errno.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Ring buffer.
 * @defgroup kernel_utils_ring Ring Buffer
 * @ingroup kernel_utils
 *
 * @{
 */

/**
 * @brief Ring buffer structure.
 * @struct ring_t
 */
typedef struct ring
{
    uint8_t* buffer; ///< Pointer to the buffer memory.
    uint64_t size; ///< The total size of the buffer.
    uint64_t head; ///< The position to write to.
    uint64_t tail; ///< The position to start reading from.
} ring_t;

/**
 * @brief Create a ring buffer initializer.
 * 
 * @param _buf Pointer to the buffer memory.
 * @param _size The size of the buffer in bytes.
 */
#define RING_CREATE(_buf, _size) \
    { \
        .buffer = (uint8_t*)(_buf), .size = (_size), .head = 0, .tail = 0 \
    }

/**
 * @brief Define and initialize a ring buffer.
 *
 * Helps define a ring buffer with a backing buffer.
 *  
 * @param _name The name of the ring buffer.
 * @param _size The size of the ring buffer in bytes.
 */
#define RING_DEFINE(_name, _size) \
    uint8_t _name##_buffer[_size]; \
    ring_t _name = RING_CREATE(_name##_buffer, _size)

/**
 * @brief Initialize a ring buffer.
 * 
 * @param ring Pointer to the ring buffer structure.
 * @param buffer Pointer to the buffer memory.
 * @param size The size of the buffer in bytes.
 */
static inline void ring_init(ring_t* ring, uint8_t* buffer, uint64_t size)
{
    ring->buffer = buffer;
    ring->size = size;
    ring->head = 0;
    ring->tail = 0;
}

/**
 * @brief Reset a ring buffer.
 * 
 * @param ring Pointer to the ring buffer structure.
 */
static inline void ring_reset(ring_t* ring)
{
    ring->head = 0;
    ring->tail = 0;
}

/**
 * @brief Return the number of bytes available for writing in a ring buffer.
 * 
 * @param ring Pointer to the ring buffer structure.
 * @param offset Pointer to the offset to check from, set to `NULL` to check from head.
 * @return The number of bytes available for writing.
 */
static inline uint64_t ring_bytes_free(const ring_t* ring, const uint64_t* offset)
{
    if (offset != NULL)
    {
        if (*offset >= ring->size - 1)
        {
            return 0;
        }
        return ring->size - *offset - 1;
    }

    uint64_t used = (ring->head >= ring->tail) ? (ring->head - ring->tail) : (ring->size - (ring->tail - ring->head));
    return ring->size - used - 1;
}

/**
 * @brief Return the number of bytes used in a ring buffer.
 * 
 * @param ring Pointer to the ring buffer structure.
 * @param offset Pointer to the offset to check from, set to `NULL` to check from tail.
 * @return The number of bytes used.
 */
static inline uint64_t ring_bytes_used(const ring_t* ring, const uint64_t* offset)
{
    uint64_t used = (ring->head >= ring->tail) ? (ring->head - ring->tail) : (ring->size - (ring->tail - ring->head));

    if (offset != NULL)
    {
        if (*offset >= used)
        {
            return 0;
        }
        return used - *offset;
    }

    return used;
}

/**
 * @brief Read data from a ring buffer at a specific offset.
 *
 * @param ring The ring buffer structure.
 * @param buffer The destination buffer.
 * @param count The number of bytes to read.
 * @param offset Pointer to the offset to read from, will be updated, set to `NULL` to read at tail.
 * @return The number of bytes read.
 */
static inline uint64_t ring_read(ring_t* ring, void* buffer, uint64_t count, uint64_t* offset)
{
    uint64_t used = ring_bytes_used(ring, NULL);
    uint64_t relative = (offset != NULL) ? *offset : 0;

    if (count == 0 || relative >= used)
    {
        return 0;
    }

    uint64_t available = used - relative;
    uint64_t bytesToRead = count;
    if (bytesToRead > available)
    {
        bytesToRead = available;
    }

    uint64_t absolute = (ring->tail + relative) % ring->size;
    uint64_t firstChunk = ring->size - absolute;
    if (firstChunk > bytesToRead)
    {
        firstChunk = bytesToRead;
    }

    memcpy(buffer, &ring->buffer[absolute], firstChunk);
    memcpy((uint8_t*)buffer + firstChunk, &ring->buffer[0], bytesToRead - firstChunk);

    if (offset != NULL)
    {
        *offset += bytesToRead;
    }
    else
    {
        ring->tail = (ring->tail + bytesToRead) % ring->size;
    }

    return bytesToRead;
}

/**
 * @brief Write data to a ring buffer at a specific offset.
 *
 * @param ring Pointer to the ring buffer structure.
 * @param buffer The source buffer.
 * @param count The number of bytes to write.
 * @param offset Pointer to the offset to write to, will be updated, set to `NULL` to write at head.
 * @return The number of bytes written.
 */
static inline uint64_t ring_write(ring_t* ring, const void* buffer, uint64_t count, uint64_t* offset)
{
    uint64_t used = ring_bytes_used(ring, NULL);
    uint64_t relative = (offset != NULL) ? *offset : used;

    if (count == 0 || relative >= ring->size - 1)
    {
        return 0;
    }

    uint64_t available = ring->size - relative - 1;
    uint64_t bytesToWrite = count;
    if (bytesToWrite > available)
    {
        bytesToWrite = available;
    }

    uint64_t absolute = (ring->tail + relative) % ring->size;
    uint64_t firstChunk = ring->size - absolute;
    if (firstChunk > bytesToWrite)
    {
        firstChunk = bytesToWrite;
    }

    memcpy(&ring->buffer[absolute], buffer, firstChunk);
    memcpy(&ring->buffer[0], (const uint8_t*)buffer + firstChunk, bytesToWrite - firstChunk);

    if (offset != NULL)
    {
        *offset += bytesToWrite;
    }

    if (relative + bytesToWrite > used)
    {
        ring->head = (ring->tail + relative + bytesToWrite) % ring->size;
    }

    return bytesToWrite;
}

/**
 * @brief Advance the head of the ring buffer.
 *
 * @param ring Pointer to the ring buffer structure.
 * @param count The number of bytes to advance the head by.
 */
static inline void ring_advance_head(ring_t* ring, uint64_t count)
{
    ring->head = (ring->head + count) % ring->size;
}

/**
 * @brief Advance the tail of the ring buffer.
 *
 * @param ring Pointer to the ring buffer structure.
 * @param count The number of bytes to advance the tail by.
 */
static inline void ring_advance_tail(ring_t* ring, uint64_t count)
{
    ring->tail = (ring->tail + count) % ring->size;
}

/** @} */

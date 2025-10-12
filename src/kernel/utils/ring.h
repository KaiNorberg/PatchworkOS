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
 */
typedef struct
{
    void* buffer;
    uint64_t size;
    uint64_t readIndex;
    uint64_t writeIndex;
    uint64_t dataLength;
} ring_t;

/**
 * @brief Create a ring buffer initializer.
 *
 * @param bufferPtr Pointer to the buffer memory.
 * @param bufferSize Size of the buffer memory in bytes.
 */
#define RING_CREATE(bufferPtr, bufferSize) \
    {.buffer = bufferPtr, .size = bufferSize, .readIndex = 0, .writeIndex = 0, .dataLength = 0}

/**
 * @brief Initialize a ring buffer.
 *
 * @param ring Pointer to the ring buffer to initialize.
 * @param buffer Pointer to the buffer memory.
 * @param size Size of the buffer memory in bytes.
 */
static inline void ring_init(ring_t* ring, void* buffer, uint64_t size)
{
    ring->buffer = buffer;
    ring->size = size;
    ring->readIndex = 0;
    ring->writeIndex = 0;
    ring->dataLength = 0;
}

/**
 * @brief Get the length of data currently stored in the ring buffer.
 *
 * @param ring Pointer to the ring buffer.
 * @return Length of data in bytes.
 */
static inline uint64_t ring_data_length(const ring_t* ring)
{
    return ring->dataLength;
}

/**
 * @brief Get the length of free space in the ring buffer.
 *
 * @param ring Pointer to the ring buffer.
 * @return Length of free space in bytes.
 */
static inline uint64_t ring_free_length(const ring_t* ring)
{
    return ring->size - ring->dataLength;
}

/**
 * @brief Write data to the ring buffer.
 *
 * If the data to be written exceeds the free space in the buffer,
 * the oldest data will be overwritten.
 *
 * @param ring Pointer to the ring buffer.
 * @param buffer Pointer to the data to write.
 * @param count Number of bytes to write.
 * @return Number of bytes written.
 */
static inline uint64_t ring_write(ring_t* ring, const void* buffer, uint64_t count)
{
    if (count > ring_free_length(ring))
    {
        uint64_t overflow = count - ring_free_length(ring);
        ring->readIndex = (ring->readIndex + overflow) % ring->size;
        ring->dataLength -= overflow;
    }

    uint64_t upperHalfSize = ring->size - ring->writeIndex;
    if (count < upperHalfSize)
    {
        memcpy((void*)((uint64_t)ring->buffer + ring->writeIndex), buffer, count);
    }
    else
    {
        memcpy((void*)((uint64_t)ring->buffer + ring->writeIndex), buffer, upperHalfSize);

        uint64_t lowerHalfSize = count - upperHalfSize;
        memcpy(ring->buffer, (void*)((uint64_t)buffer + upperHalfSize), lowerHalfSize);
    }

    ring->writeIndex = (ring->writeIndex + count) % ring->size;
    ring->dataLength += count;

    return count;
}

/**
 * @brief Read data from the ring buffer.
 *
 * @param ring Pointer to the ring buffer.
 * @param buffer Pointer to the buffer to store the read data.
 * @param count Number of bytes to read.
 * @return Number of bytes read, or ERR if not enough data is available.
 */
static inline uint64_t ring_read(ring_t* ring, void* buffer, uint64_t count)
{
    if (count > ring_data_length(ring))
    {
        return ERR;
    }

    uint64_t upperHalfSize = ring->size - ring->readIndex;
    if (count < upperHalfSize)
    {
        memcpy(buffer, (void*)((uint64_t)ring->buffer + ring->readIndex), count);
    }
    else
    {
        memcpy(buffer, (void*)((uint64_t)ring->buffer + ring->readIndex), upperHalfSize);

        uint64_t lowerHalfSize = count - upperHalfSize;
        memcpy((void*)((uint64_t)buffer + upperHalfSize), ring->buffer, lowerHalfSize);
    }

    ring->readIndex = (ring->readIndex + count) % ring->size;
    ring->dataLength -= count;

    return count;
}

/**
 * @brief Read data from the ring buffer at a specific offset without modifying the read index.
 *
 * @param ring Pointer to the ring buffer.
 * @param offset Offset from the current read index to start reading.
 * @param buffer Pointer to the buffer to store the read data.
 * @param count Number of bytes to read.
 * @return Number of bytes read, or 0 if offset is out of bounds.
 */
static inline uint64_t ring_read_at(const ring_t* ring, uint64_t offset, void* buffer, uint64_t count)
{
    uint64_t availableBytes = (offset >= ring->dataLength) ? 0 : (ring->dataLength - offset);
    count = (count > availableBytes) ? availableBytes : count;

    if (count == 0)
    {
        return 0;
    }

    uint64_t pos = (ring->readIndex + offset) % ring->size;

    uint64_t upperHalfSize = ring->size - pos;
    if (count < upperHalfSize)
    {
        memcpy(buffer, (void*)((uint64_t)ring->buffer + pos), count);
    }
    else
    {
        memcpy(buffer, (void*)((uint64_t)ring->buffer + pos), upperHalfSize);
        uint64_t lowerHalfSize = count - upperHalfSize;
        memcpy((void*)((uint64_t)buffer + upperHalfSize), ring->buffer, lowerHalfSize);
    }

    return count;
}

/**
 * @brief Move the read index forward by a specified offset.
 *
 * If the offset exceeds the current data length, no action is taken.
 *
 * @param ring Pointer to the ring buffer.
 * @param offset Number of bytes to move the read index forward.
 */
static void ring_move_read_forward(ring_t* ring, uint64_t offset)
{
    if (offset > ring_data_length(ring))
    {
        return;
    }

    ring->readIndex = (ring->readIndex + offset) % ring->size;
    ring->dataLength -= offset;
}

/**
 * @brief Get a byte from the ring buffer at a specific offset without modifying the read index.
 *
 * @param ring Pointer to the ring buffer.
 * @param offset Offset from the current read index to get the byte.
 * @param byte Pointer to store the retrieved byte.
 * @return 0 on success, or `ERR` if offset is out of bounds.
 */
static inline uint64_t ring_get_byte(const ring_t* ring, uint64_t offset, uint8_t* byte)
{
    if (offset >= ring->dataLength)
    {
        return ERR;
    }

    uint64_t byteIndex = (ring->readIndex + offset) % ring->size;
    *byte = ((uint8_t*)ring->buffer)[byteIndex];
    return 0;
}

/** @} */

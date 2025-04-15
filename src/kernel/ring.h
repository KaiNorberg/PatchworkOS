#pragma once

#include "defs.h"

#include <stdint.h>
#include <string.h>

typedef struct
{
    void* buffer;
    uint64_t size;
    uint64_t readIndex;
    uint64_t writeIndex;
    uint64_t dataLength;
} ring_t;

static inline void ring_init(ring_t* ring, void* buffer, uint64_t size)
{
    ring->buffer = buffer;
    ring->size = size;
    ring->readIndex = 0;
    ring->writeIndex = 0;
    ring->dataLength = 0;
}

static inline uint64_t ring_data_length(ring_t* ring)
{
    return ring->dataLength;
}

static inline uint64_t ring_free_length(ring_t* ring)
{
    return ring->size - ring->dataLength;
}

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

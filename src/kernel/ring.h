#pragma once

#include "pmm.h"

#include <stdint.h>

#define RING_SIZE 100

typedef struct
{
    void* buffer;
    uint64_t readIndex;
    uint64_t writeIndex;
    uint64_t dataLength;
} ring_t;

static inline void ring_init(ring_t* ring)
{
    ring->buffer = pmm_alloc();
    ring->readIndex = 0;
    ring->writeIndex = 0;
    ring->dataLength = 0;
}

static inline void ring_deinit(ring_t* ring)
{
    pmm_free(ring->buffer);
}

static inline uint64_t ring_data_length(ring_t* ring)
{
    return ring->dataLength;
}

static inline uint64_t ring_free_length(ring_t* ring)
{
    return RING_SIZE - ring->dataLength;
}

static inline uint64_t ring_write(ring_t* ring, const void* buffer, uint64_t count)
{
    if (count > ring_free_length(ring))
    {
        return ERR;
    }

    uint64_t upperHalfSize = RING_SIZE - ring->writeIndex;
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

    ring->writeIndex = (ring->writeIndex + count) % RING_SIZE;
    ring->dataLength += count;

    return count;
}

static inline uint64_t ring_read(ring_t* ring, void* buffer, uint64_t count)
{
    if (count > ring_data_length(ring))
    {
        return ERR;
    }

    uint64_t upperHalfSize = RING_SIZE - ring->readIndex;
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

    ring->readIndex = (ring->readIndex + count) % RING_SIZE;
    ring->dataLength -= count;

    return count;
}

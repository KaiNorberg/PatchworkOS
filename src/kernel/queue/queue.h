#pragma once

#include <stdint.h>

#include "lock/lock.h"

#define QUEUE_INITIAL_LENGTH 4

typedef struct
{
    void** buffer;
    uint64_t bufferLength;
    uint64_t readIndex;
    uint64_t writeIndex;
    Lock lock;    
    _Atomic uint64_t length;
} Queue;

Queue* queue_new();

void queue_free(Queue* queue);

void queue_push(Queue* queue, void* item);

void* queue_pop(Queue* queue);

uint64_t queue_length(Queue* queue);
#pragma once

#include <stdint.h>

#define QUEUE_INITIAL_SIZE 4

typedef struct
{
    uint64_t reservedLength;
    uint64_t length;

    void** data;

    uint64_t firstIndex;
    uint64_t lastIndex;
} Queue;

Queue* queue_new();

void queue_resize(Queue* queue, uint64_t newSize);

void queue_push(Queue* queue, void* item);

void* queue_pop(Queue* queue);

uint64_t queue_length(Queue* queue);

void queue_free(Queue* queue);
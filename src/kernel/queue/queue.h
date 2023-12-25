#pragma once

#include <stdint.h>

typedef struct QueueEntry
{
    void* data;
    struct QueueEntry* next;
    struct QueueEntry* prev;
} QueueEntry;

typedef struct
{
    uint64_t entryAmount;

    QueueEntry* firstEntry;
    QueueEntry* lastEntry;
} Queue;

//TODO: Optimize queue, dont immediately free popped entires

void queue_visualize(Queue* queue);

Queue* queue_new();

void queue_push(Queue* queue, void* item);

void* queue_pop(Queue* queue);

uint64_t queue_length(Queue* queue);

void queue_free(Queue* queue);
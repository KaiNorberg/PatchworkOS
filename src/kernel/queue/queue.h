#pragma once

#include "defs/defs.h"
#include "lock/lock.h"
#include "list/list.h"

typedef struct
{
    uint64_t length;
    List list;
    Lock lock;
} Queue;

static inline void queue_init(Queue* queue)
{
    queue->length = 0;
    list_init(&queue->list);
    lock_init(&queue->lock);
}

static inline void queue_push(Queue* queue, void* element)
{
    LOCK_GUARD(&queue->lock);

    queue->length++;
    list_push(&queue->list, element);
}

static inline void* queue_pop(Queue* queue)
{
    LOCK_GUARD(&queue->lock);
    if (queue->length == 0)
    {
        return NULL;
    }

    queue->length--;
    return list_pop(&queue->list);
}

static inline uint64_t queue_length(Queue const* queue)
{
    return queue->length;
}
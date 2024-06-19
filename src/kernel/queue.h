#pragma once

#include "defs.h"
#include "list.h"
#include "lock.h"

typedef struct
{
    uint64_t length;
    list_t list;
    lock_t lock;
} queue_t;

static inline void queue_init(queue_t* queue)
{
    queue->length = 0;
    list_init(&queue->list);
    lock_init(&queue->lock);
}

static inline void queue_push(queue_t* queue, void* element)
{
    LOCK_GUARD(&queue->lock);

    queue->length++;
    list_push(&queue->list, element);
}

static inline void* queue_pop(queue_t* queue)
{
    LOCK_GUARD(&queue->lock);
    if (queue->length == 0)
    {
        return NULL;
    }

    queue->length--;
    return list_pop(&queue->list);
}

static inline uint64_t queue_length(queue_t const* queue)
{
    return queue->length;
}

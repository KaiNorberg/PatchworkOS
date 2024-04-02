#include "queue.h"

#include "heap/heap.h"

static inline void* queue_pop_unlocked(Queue* queue)
{
    if (queue->length == 0)
    {
        return NULL;
    }

    void* temp = queue->buffer[queue->readIndex];
    queue->length--;
    queue->readIndex = (queue->readIndex + 1) % queue->capacity;

    return temp;
}

static inline void queue_resize_unlocked(Queue* queue, uint64_t capacity)
{
    void** newBuffer = kmalloc(capacity * sizeof(void*));

    uint64_t oldLength = queue->length;
    for (uint64_t i = 0; i < oldLength; i++)
    {
        newBuffer[i] = queue_pop_unlocked(queue);
    }

    kfree(queue->buffer);

    queue->length = oldLength;
    queue->buffer = newBuffer;
    queue->capacity = capacity;
    queue->readIndex = 0;
    queue->writeIndex = oldLength;
}

static inline void queue_push_unlocked(Queue* queue, void* item)
{
    if (queue->capacity == queue->length)
    {
        queue_resize_unlocked(queue, queue->capacity * 2);
    }

    queue->buffer[queue->writeIndex] = item;
    queue->length++;
    queue->writeIndex = (queue->writeIndex + 1) % queue->capacity;
}


Queue* queue_new()
{
    Queue* queue = kmalloc(sizeof(Queue));

    queue->buffer = kmalloc(QUEUE_INITIAL_LENGTH * sizeof(void*));
    queue->capacity = QUEUE_INITIAL_LENGTH;
    queue->readIndex = 0;
    queue->writeIndex = 0;
    queue->length = 0;
    queue->lock = lock_create();

    return queue;
}

void queue_free(Queue* queue)
{
    kfree(queue->buffer);
    kfree(queue);
}

void queue_push(Queue* queue, void* item)
{
    lock_acquire(&queue->lock);
    queue_push_unlocked(queue, item);
    lock_release(&queue->lock);
}

void* queue_pop(Queue* queue)
{
    lock_acquire(&queue->lock);
    void* temp = queue_pop_unlocked(queue);
    lock_release(&queue->lock);
    return temp;
}

uint64_t queue_length(Queue* queue)
{
    lock_acquire(&queue->lock);
    uint64_t temp = queue->length;
    lock_release(&queue->lock);
    return temp;
}
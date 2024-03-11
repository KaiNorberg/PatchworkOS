#include "queue.h"

#include "heap/heap.h"

static inline void* queue_pop_unlocked(Queue* queue)
{
    if (atomic_load(&queue->length) == 0)
    {
        return 0;
    }
    
    void* temp = queue->buffer[queue->readIndex];
    atomic_fetch_sub(&queue->length, 1);

    queue->readIndex++;
    queue->readIndex %= queue->bufferLength;

    return temp;
}

static inline void queue_resize_unlocked(Queue* queue, uint64_t bufferLength)
{
    void** newBuffer = kmalloc(bufferLength * sizeof(void*));

    uint64_t i = 0;
    while (i != atomic_load(&queue->length))
    {
        newBuffer[i] = queue_pop_unlocked(queue);
        i++;
    }

    kfree(queue->buffer);

    queue->buffer = newBuffer;
    queue->bufferLength = bufferLength;
    queue->readIndex = 0;
    queue->writeIndex = i;
}

static inline void queue_push_unlocked(Queue* queue, void* item)
{
    if (queue->bufferLength == atomic_load(&queue->length))
    {
        queue_resize_unlocked(queue, queue->bufferLength * 2);
    }

    queue->buffer[queue->writeIndex] = item;
    atomic_fetch_add(&queue->length, 1);

    queue->writeIndex++;
    queue->writeIndex %= queue->bufferLength;
}

Queue* queue_new()
{
    Queue* newQueue = kmalloc(sizeof(Queue));

    newQueue->buffer = kmalloc(QUEUE_INITIAL_LENGTH * sizeof(void*));
    newQueue->bufferLength = QUEUE_INITIAL_LENGTH;
    
    newQueue->readIndex = 0;
    newQueue->writeIndex = 0;

    newQueue->lock = lock_new();

    newQueue->length = 0;

    return newQueue;
}

void queue_free(Queue* queue)
{
    //Wait until its done being used
    lock_acquire(&queue->lock);

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
    return atomic_load(&queue->length);
}
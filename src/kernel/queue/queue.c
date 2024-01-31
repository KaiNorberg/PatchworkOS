#include "queue.h"

#include "heap/heap.h"

#include "tty/tty.h"

Queue* queue_new()
{
    Queue* newQueue = kmalloc(sizeof(Queue));
    
    newQueue->reservedLength = QUEUE_INITIAL_LENGTH;
    newQueue->length = 0;

    newQueue->data = kmalloc(QUEUE_INITIAL_LENGTH * sizeof(void*));

    newQueue->firstIndex = 0;
    newQueue->lastIndex = 0;

    return newQueue;
}

void queue_resize(Queue* queue, uint64_t newLength)
{
    void** newData = kmalloc(newLength * sizeof(void*));

    uint64_t length = queue_length(queue);
    uint64_t i = 0;
    while (queue_length(queue) != 0)
    {
        newData[i] = queue_pop(queue);
        i++;
    }

    kfree(queue->data);

    queue->length = length;
    queue->reservedLength = newLength;
    queue->data = newData;
    queue->firstIndex = 0;
    queue->lastIndex = i;
}

void queue_push(Queue* queue, void* item)
{        
    if (queue->reservedLength == queue->length)
    {
        queue_resize(queue, queue->reservedLength * 2);
    }

    queue->data[queue->lastIndex] = item;
    queue->length++;

    queue->lastIndex++;
    queue->lastIndex %= queue->reservedLength;
}

void* queue_pop(Queue* queue)
{   
    if (queue->length == 0)
    {
        return 0;
    }
    
    void* temp = queue->data[queue->firstIndex];
    queue->length--;

    queue->firstIndex++;
    queue->firstIndex %= queue->reservedLength;

    return temp;
}

uint64_t queue_length(Queue const* queue)
{
    return queue->length;
}

void queue_free(Queue* queue)
{
    kfree(queue->data);
    kfree(queue);
}
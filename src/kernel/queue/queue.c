#include "queue.h"

#include "heap/heap.h"

#include "tty/tty.h"

Queue* queue_new()
{
    Queue* newQueue = kmalloc(sizeof(Queue));
    
    newQueue->reservedLength = QUEUE_INITIAL_SIZE;
    newQueue->length = 0;

    newQueue->data = kmalloc(QUEUE_INITIAL_SIZE * sizeof(void*));

    newQueue->firstIndex = 0;
    newQueue->lastIndex = 0;

    return newQueue;
}

void queue_push(Queue* queue, void* item)
{        
    if (queue->reservedLength == queue->length)
    {
        uint64_t newReservedLength = queue->reservedLength * 2;
        void** newData = kmalloc(newReservedLength * sizeof(void*));

        uint64_t length = queue_length(queue);
        uint64_t i = 0;
        while (queue_length(queue) != 0)
        {
            newData[i] = queue_pop(queue);
            i++;
        }

        kfree(queue->data);

        queue->length = length;
        queue->reservedLength = newReservedLength;
        queue->data = newData;
        queue->firstIndex = 0;
        queue->lastIndex = i;
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

uint64_t queue_length(Queue* queue)
{
    return queue->length;
}

void queue_free(Queue* queue)
{
    kfree(queue->data);
    kfree(queue);
}
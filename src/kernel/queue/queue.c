#include "queue.h"

#include "heap/heap.h"

#include "tty/tty.h"

void queue_visualize(Queue* queue)
{
    tty_print("Queue visualization: ");
    tty_print("Length: "); tty_printx(queue->length); 
    tty_print(" First Index: "); tty_printx(queue->firstIndex); 
    tty_print(" Last Index: "); tty_printx(queue->lastIndex); 
    tty_print("\n\r");

    Pixel black;
    black.a = 255;
    black.r = 0;
    black.g = 0;
    black.b = 0;

    Pixel green;
    green.a = 255;
    green.r = 152;
    green.g = 195;
    green.b = 121;

    Pixel red;
    red.a = 255;
    red.r = 224;
    red.g = 108;
    red.b = 117;

    Pixel blue;
    blue.a = 255;
    blue.r = 0;
    blue.g = 0;
    blue.b = 255;

    for (uint64_t i = 0; i < queue->reservedLength; i++)
    {
        if (i == queue->firstIndex && i == queue->lastIndex)
        {
            tty_set_background(blue);
        }
        else if (i == queue->firstIndex)
        {
            tty_set_background(black);
        }
        else if (i == queue->lastIndex)
        {
            tty_set_background(red);
        }
        else
        {
            tty_set_background(green);
        }
        
        tty_put(' ');
        tty_printx((uint64_t)queue->data[i]);
        tty_put(' ');
    }

    tty_print("\n\r");
    tty_set_background(black);
}

Queue* queue_new()
{
    Queue* newQueue = kmalloc(sizeof(Queue));
    
    newQueue->reservedLength = QUEUE_INITIAL_SIZE;
    newQueue->length = 0;

    newQueue->data = kmalloc(QUEUE_INITIAL_SIZE * sizeof(void*));

    newQueue->firstIndex = QUEUE_INITIAL_SIZE - 1;
    newQueue->lastIndex = QUEUE_INITIAL_SIZE - 1;

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
            newData[newReservedLength - 1 - i] = queue_pop(queue);
            i++;
        }

        kfree(queue->data);

        queue->length = length;
        queue->reservedLength = newReservedLength;
        queue->data = newData;
        queue->firstIndex = newReservedLength - 1;
        queue->lastIndex = queue->firstIndex - i;
    }

    queue->data[queue->lastIndex] = item;
    queue->length++;

    if (queue->lastIndex == 0)
    {
        queue->lastIndex = queue->reservedLength - 1;
    }
    else
    {
        queue->lastIndex--;
    }
}

void* queue_pop(Queue* queue)
{   
    if (queue->length == 0)
    {
        return -1;
    }

    void* temp = queue->data[queue->firstIndex];
    queue->length--;

    if (queue->firstIndex == 0)
    {
        queue->firstIndex = queue->reservedLength - 1;
    }
    else
    {
        queue->firstIndex--;
    }

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
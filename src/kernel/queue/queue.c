#include "queue.h"

#include "heap/heap.h"

#include "tty/tty.h"

void queue_visualize(Queue* queue)
{
    tty_print("Queue visualization: \n\r");

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

    tty_print("Traversed forword: ");
    tty_set_background(green);

    QueueEntry* currentEntry = queue->firstEntry;
    while (1)
    {
        if (currentEntry == 0)
        {
            break;
        }

        tty_put(' '); tty_printx((uint64_t)currentEntry->data);
        currentEntry = currentEntry->next;
    }
    tty_print(" \n\r");
    tty_set_background(black);
    tty_print("Traversed backword: ");
    tty_set_background(green);

    currentEntry = queue->lastEntry;
    while (1)
    {
        if (currentEntry == 0)
        {
            break;
        }

        tty_put(' '); tty_printx((uint64_t)currentEntry->data);
        currentEntry = currentEntry->prev;
    }    
    tty_print(" \n\n\r");

    tty_set_background(black);
}

Queue* queue_new()
{
    Queue* newQueue = kmalloc(sizeof(Queue));
    
    newQueue->firstEntry = 0;
    newQueue->lastEntry = 0;
    newQueue->entryAmount = 0;

    return newQueue;
}

void queue_push(Queue* queue, void* item)
{        
    QueueEntry* newEntry = kmalloc(sizeof(QueueEntry));
    newEntry->data = item;
    newEntry->next = 0;

    if (queue->entryAmount == 0)
    {
        newEntry->prev = 0;

        queue->firstEntry = newEntry;
        queue->lastEntry = newEntry;

        queue->entryAmount = 1;        
    }
    else
    {
        newEntry->prev = queue->lastEntry;

        queue->lastEntry->next = newEntry;
        queue->lastEntry = newEntry;

        queue->entryAmount++;     
    }
}

void* queue_pop(Queue* queue)
{   
    if (queue->entryAmount == 0)
    {
        return 0;
    }
    else if (queue->entryAmount == 1)
    {
        void* data = queue->firstEntry->data;
        QueueEntry* firstEntry = queue->firstEntry;

        queue->entryAmount = 0;
        queue->firstEntry = 0;
        queue->lastEntry = 0;
        
        kfree(firstEntry);

        return data;
    }
    else
    {
        void* data = queue->firstEntry->data;
        QueueEntry* firstEntry = queue->firstEntry;

        queue->firstEntry->next->prev = 0;
        queue->firstEntry = queue->firstEntry->next;

        kfree(firstEntry);

        queue->entryAmount--;

        return data;
    }
}

uint64_t queue_length(Queue* queue)
{
    return queue->entryAmount;
}

void queue_free(Queue* queue)
{
    QueueEntry* currentEntry = queue->firstEntry;
    while (1)
    {
        if (currentEntry == 0)
        {
            break;
        }

        QueueEntry* nextEntry = currentEntry->next;

        kfree(currentEntry);

        currentEntry = nextEntry;
    }

    kfree(queue);
}
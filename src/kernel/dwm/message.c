#include "message.h"
#include "lock.h"

#include <string.h>

void message_queue_init(message_queue_t* queue)
{
    memset(queue->queue, 0, sizeof(queue->queue));
    queue->readIndex = 0;
    queue->writeIndex = 0;
    lock_init(&queue->lock);
}

bool message_queue_avail(message_queue_t* queue)
{
    LOCK_GUARD(&queue->lock);

    return queue->readIndex != queue->writeIndex;
}

void message_queue_push(message_queue_t* queue, msg_t type, const void* data, uint64_t size)
{
    LOCK_GUARD(&queue->lock);

    message_t message = (message_t){
        .size = size,
        .type = type,
    };
    if (data != NULL)
    {
        memcpy(message.data, data, size);
    }

    queue->queue[queue->writeIndex] = message;
    queue->writeIndex = (queue->writeIndex + 1) % MESSAGE_QUEUE_MAX;
}

bool message_queue_pop(message_queue_t* queue, message_t* out)
{
    LOCK_GUARD(&queue->lock);

    if (queue->readIndex == queue->writeIndex)
    {
        return false;
    }

    *out = queue->queue[queue->readIndex];
    queue->readIndex = (queue->readIndex + 1) % MESSAGE_QUEUE_MAX;

    return true;
}

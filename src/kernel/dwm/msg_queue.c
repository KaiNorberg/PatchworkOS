#include "msg_queue.h"
#include "lock.h"

#include <string.h>

void msg_queue_init(msg_queue_t* queue)
{
    memset(queue->queue, 0, sizeof(queue->queue));
    queue->readIndex = 0;
    queue->writeIndex = 0;
    lock_init(&queue->lock);
}

bool msg_queue_avail(msg_queue_t* queue)
{
    LOCK_GUARD(&queue->lock);

    return queue->readIndex != queue->writeIndex;
}

void msg_queue_push(msg_queue_t* queue, const msg_t* msg)
{
    LOCK_GUARD(&queue->lock);

    queue->queue[queue->writeIndex] = *msg;
    queue->writeIndex = (queue->writeIndex + 1) % MSG_QUEUE_MAX;
}

void msg_queue_push_empty(msg_queue_t* queue, uint16_t type)
{
    msg_t msg = {.type = type};
    msg_queue_push(queue, &msg);
}

bool msg_queue_pop(msg_queue_t* queue, msg_t* msg)
{
    LOCK_GUARD(&queue->lock);

    if (queue->readIndex == queue->writeIndex)
    {
        return false;
    }

    *msg = queue->queue[queue->readIndex];
    queue->readIndex = (queue->readIndex + 1) % MSG_QUEUE_MAX;

    return true;
}

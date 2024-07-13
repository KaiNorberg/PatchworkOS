#include "msg_queue.h"

#include "lock.h"
#include "time.h"

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

void msg_queue_push(msg_queue_t* queue, msg_type_t type, const void* data, uint64_t size)
{
    LOCK_GUARD(&queue->lock);

    msg_t* msg = &queue->queue[queue->writeIndex];
    msg->type = type;
    msg->time = time_uptime();
    memcpy(msg->data, data, size);

    queue->writeIndex = (queue->writeIndex + 1) % MSG_QUEUE_MAX;
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

#include "msg_queue.h"

#include "lock.h"
#include "sched.h"
#include "systime.h"

#include <string.h>

void msg_queue_init(msg_queue_t* queue)
{
    memset(queue->queue, 0, sizeof(queue->queue));
    queue->readIndex = 0;
    queue->writeIndex = 0;
    wait_queue_init(&queue->waitQueue);
    lock_init(&queue->lock);
}

void msg_queue_deinit(msg_queue_t* queue)
{
    wait_queue_deinit(&queue->waitQueue);
}

bool msg_queue_avail(msg_queue_t* queue)
{
    LOCK_DEFER(&queue->lock);
    return queue->readIndex != queue->writeIndex;
}

void msg_queue_push(msg_queue_t* queue, msg_type_t type, const void* data, uint64_t size)
{
    LOCK_DEFER(&queue->lock);

    msg_t* msg = &queue->queue[queue->writeIndex];
    msg->type = type;
    msg->time = systime_uptime();
    memcpy(msg->data, data, size);
    queue->writeIndex = (queue->writeIndex + 1) % MSG_QUEUE_MAX;

    waitsys_unblock(&queue->waitQueue, WAITSYS_ALL);
}

void msg_queue_pop(msg_queue_t* queue, msg_t* msg, nsec_t timeout)
{
    if (WAITSYS_BLOCK_LOCK_TIMEOUT(&queue->waitQueue, &queue->lock, queue->readIndex != queue->writeIndex, timeout) != BLOCK_NORM)
    {
        *msg = (msg_t){.type = MSG_NONE};
        lock_release(&queue->lock);
        return;
    }

    *msg = queue->queue[queue->readIndex];
    queue->readIndex = (queue->readIndex + 1) % MSG_QUEUE_MAX;
    lock_release(&queue->lock);
}

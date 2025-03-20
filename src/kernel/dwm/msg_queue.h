#pragma once

#include "lock.h"
#include "sched.h"

#include <sys/dwm.h>

#define MSG_QUEUE_MAX 8

typedef struct msg_queue
{
    msg_t queue[MSG_QUEUE_MAX];
    uint8_t readIndex;
    uint8_t writeIndex;
    blocker_t blocker;
    lock_t lock;
} msg_queue_t;

void msg_queue_init(msg_queue_t* queue);

void msg_queue_deinit(msg_queue_t* queue);

bool msg_queue_avail(msg_queue_t* queue);

void msg_queue_push(msg_queue_t* queue, msg_type_t type, const void* data, uint64_t size);

void msg_queue_pop(msg_queue_t* queue, msg_t* msg, nsec_t timeout);

#pragma once

#include "lock.h"

#include <sys/win.h>

#define MSG_QUEUE_MAX 32

typedef struct msg_queue
{
    msg_t queue[MSG_QUEUE_MAX];
    uint64_t readIndex;
    uint64_t writeIndex;
    lock_t lock;
} msg_queue_t;

void msg_queue_init(msg_queue_t* queue);

bool msg_queue_avail(msg_queue_t* queue);

void msg_queue_push(msg_queue_t* queue, const msg_t* msg);

void msg_queue_push_empty(msg_queue_t* queue, uint16_t type);

bool msg_queue_pop(msg_queue_t* queue, msg_t* msg);

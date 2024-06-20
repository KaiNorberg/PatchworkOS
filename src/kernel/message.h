#pragma once

#include "lock.h"

#include <sys/win.h>

#define MESSAGE_QUEUE_MAX 32

typedef struct message
{
    msg_t type;
    uint64_t size;
    uint8_t data[MSG_MAX_DATA];
} message_t;

typedef struct message_queue
{
    message_t queue[MESSAGE_QUEUE_MAX];
    uint64_t readIndex;
    uint64_t writeIndex;
    lock_t lock;
} message_queue_t;

void message_queue_init(message_queue_t* queue);

void message_queue_push(message_queue_t* queue, msg_t type, const void* data, uint64_t size);

bool message_queue_pop(message_queue_t* queue, message_t* out);

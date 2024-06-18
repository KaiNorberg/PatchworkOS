#pragma once

#include "defs.h"
#include "list.h"
#include "lock.h"

#include <common/boot_info.h>

#include <sys/win.h>

#define MESSAGE_QUEUE_MAX 32

typedef struct
{
    msg_t type;
    uint64_t size;
    uint8_t data[MSG_MAX_DATA];
} Message;

typedef struct
{
    Message queue[MESSAGE_QUEUE_MAX];
    uint64_t readIndex;
    uint64_t writeIndex;
    Lock lock;
} MessageQueue;

typedef struct
{
    ListEntry base;
    uint64_t x;
    uint64_t y;
    uint64_t height;
    uint64_t width;
    pixel_t* buffer;
    win_type_t type;
    Lock lock;
    MessageQueue messages;
} Window;

void dwm_init(GopBuffer* gopBuffer);

void dwm_start(void);

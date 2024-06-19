#pragma once

#include "defs.h"
#include "list.h"
#include "lock.h"

#include <common/boot_info.h>

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

typedef struct window
{
    list_entry_t base;
    uint64_t x;
    uint64_t y;
    uint64_t height;
    uint64_t width;
    pixel_t* buffer;
    win_type_t type;
    lock_t lock;
    message_queue_t messages;
} window_t;

void dwm_init(gop_buffer_t* gopBuffer);

void dwm_start(void);

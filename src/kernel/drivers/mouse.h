#pragma once

#include "fs/sysfs.h"
#include "sched/wait.h"

#include <stdint.h>
#include <sys/mouse.h>

#define MOUSE_MAX_EVENT 32

typedef struct
{
    mouse_event_t events[MOUSE_MAX_EVENT];
    uint64_t writeIndex;
    wait_queue_t waitQueue;
    lock_t lock;
    sysfs_file_t sysfs_file;
} mouse_t;

mouse_t* mouse_new(const char* name);

void mouse_free(mouse_t* mouse);

void mouse_push(mouse_t* mouse, mouse_buttons_t buttons, int64_t deltaX, int64_t deltaY);

#pragma once

#include "sysfs.h"
#include "waitsys.h"

#include <stdint.h>
#include <sys/kbd.h>

#define KBD_MAX_EVENT 32

typedef struct
{
    kbd_event_t events[KBD_MAX_EVENT];
    uint64_t writeIndex;
    kbd_mods_t mods;
    resource_t* resource;
    wait_queue_t waitQueue;
    lock_t lock;
} kbd_t;

kbd_t* kbd_new(const char* name);

void kbd_free(kbd_t* kbd);

void kbd_push(kbd_t* kbd, kbd_event_type_t type, keycode_t code);

#pragma once

#include "config.h"
#include "simd.h"
#include "trap.h"
#include "waitsys.h"
#include "process.h"

#include <errno.h>
#include <sys/list.h>
#include <sys/proc.h>

typedef uint8_t priority_t;

#define PRIORITY_LEVELS 3
#define PRIORITY_MIN 0
#define PRIORITY_MAX (PRIORITY_LEVELS - 1)

typedef struct thread
{
    list_entry_t entry;
    process_t* process;
    tid_t id;
    bool dead;
    nsec_t timeStart;
    nsec_t timeEnd;
    waitsys_ctx_t waitsys;
    errno_t error;
    priority_t priority;
    simd_ctx_t simd;
    trap_frame_t trapFrame;
    uint8_t kernelStack[CONFIG_KERNEL_STACK];
} thread_t;

thread_t* thread_new(process_t* process, void* entry, priority_t priority);

void thread_free(thread_t* thread);

void thread_save(thread_t* thread, const trap_frame_t* trapFrame);

void thread_load(thread_t* thread, trap_frame_t* trapFrame);

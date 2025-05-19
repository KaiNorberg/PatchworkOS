#pragma once

// Note: This is effectively our "signal" system but inspired by plan9's note system.

#include "cpu/trap.h"
#include "sync/lock.h"

#include <sys/proc.h>
#include <sys/io.h>

typedef struct cpu cpu_t;

typedef enum
{
    NOTE_NONE = (1 << 0),
    NOTE_CRITICAL = (1 << 1)
} note_flags_t;

typedef struct note
{
    char message[MAX_PATH];
    pid_t sender;
    note_flags_t flags;
} note_t;

typedef struct
{
    note_t notes[CONFIG_MAX_NOTES];
    uint64_t readIndex;
    uint64_t writeIndex;
    uint64_t length;
    lock_t lock;
} note_queue_t;

void note_queue_init(note_queue_t* queue);

uint64_t note_queue_length(note_queue_t* queue);

uint64_t note_queue_push(note_queue_t* queue, const void* message, uint64_t length, note_flags_t flags);

void note_trap_handler(trap_frame_t* trapFrame, cpu_t* self);
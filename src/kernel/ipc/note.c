#include "note.h"

#include "cpu/smp.h"
#include "log/log.h"
#include "sched/thread.h"

#include <assert.h>

void note_queue_init(note_queue_t* queue)
{
    queue->readIndex = 0;
    queue->writeIndex = 0;
    queue->length = 0;
    lock_init(&queue->lock);
}

uint64_t note_queue_length(note_queue_t* queue)
{
    LOCK_DEFER(&queue->lock);
    return queue->length;
}

uint64_t note_queue_push(note_queue_t* queue, const void* message, uint64_t length, note_flags_t flags)
{
    if (length >= MAX_PATH)
    {
        return ERROR(EINVAL);
    }

    process_t* sender = sched_process();

    LOCK_DEFER(&queue->lock);

    note_t* note = NULL;
    if (queue->length == CONFIG_MAX_NOTES)
    {
        if (!(flags & NOTE_CRITICAL))
        {
            // TODO: Implement blocking
            return ERROR(EWOULDBLOCK);
        }

        // Overwrite non critical note.
        for (uint64_t i = 0; i < CONFIG_MAX_NOTES; i++)
        {
            if (!(queue->notes[i].flags & NOTE_CRITICAL))
            {
                note = &queue->notes[queue->writeIndex];
                break;
            }
        }
    }
    else
    {
        note = &queue->notes[queue->writeIndex];
        queue->writeIndex = (queue->writeIndex + 1) % CONFIG_MAX_NOTES;
        queue->length++;
    }

    assert(note != NULL);

    memcpy(note->message, message, length);
    note->message[length] = '\0';
    note->sender = sender->id;
    note->flags = flags;
    return 0;
}

static bool note_queue_pop(note_queue_t* queue, note_t* note)
{
    LOCK_DEFER(&queue->lock);

    if (queue->length == 0)
    {
        return false;
    }

    (*note) = queue->notes[queue->readIndex];
    queue->readIndex = (queue->readIndex + 1) % CONFIG_MAX_NOTES;
    queue->length--;
    return true;
}

bool note_dispatch(trap_frame_t* trapFrame, cpu_t* self)
{
    // TODO: Implement more notes and implement user space "software interrupts" to receive notes.

    thread_t* thread = sched_thread();
    note_queue_t* queue = &thread->notes;
    // log_print(LOG_INFO, "note_dispatch: %p %p\n", thread, queue);

    note_t note;
    while (note_queue_pop(queue, &note))
    {
        if (strcmp(note.message, "kill") == 0) // TODO: Fix bug.
        {
            log_print(LOG_INFO, "note: kill tid=%d pid=%d\n", thread->id, thread->process->id);

            sched_process_exit(0);
            sched_schedule(trapFrame, self);
            return true;
        }
        else
        {
            log_print(LOG_INFO, "note: unknown (%s) tid=%d pid=%d\n", note.message, thread->id, thread->process->id);
            // TODO: Unknown note, send to userspace
        }
    }

    return false;
}
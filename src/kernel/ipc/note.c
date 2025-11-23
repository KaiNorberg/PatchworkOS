#include <kernel/ipc/note.h>

#include <kernel/cpu/cpu.h>
#include <kernel/log/log.h>
#include <kernel/sched/thread.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static bool note_queue_compare_buffers(const void* a, uint64_t aLength, const void* b, uint64_t bLength)
{
    if (aLength != bLength)
    {
        return false;
    }

    return memcmp(a, b, aLength) == 0;
}

void note_queue_init(note_queue_t* queue)
{
    queue->readIndex = 0;
    queue->writeIndex = 0;
    queue->length = 0;
    queue->flags = NOTE_QUEUE_NONE;
    lock_init(&queue->lock);
}

uint64_t note_queue_length(note_queue_t* queue)
{
    LOCK_SCOPE(&queue->lock);
    return queue->length + ((queue->flags & NOTE_QUEUE_RECIEVED_KILL) ? 1 : 0);
}

uint64_t note_queue_write(note_queue_t* queue, const void* buffer, uint64_t count)
{
    if (queue == NULL || buffer == NULL || count == 0 || count >= NOTE_MAX_BUFFER)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* sender = sched_process();

    LOCK_SCOPE(&queue->lock);

    if (note_queue_compare_buffers(buffer, count, "kill", 4))
    {
        queue->flags |= NOTE_QUEUE_RECIEVED_KILL;
        return 0;
    }

    note_t* note = NULL;
    if (queue->length == CONFIG_MAX_NOTES)
    {
        note = &queue->notes[queue->readIndex];
        queue->readIndex = (queue->readIndex + 1) % CONFIG_MAX_NOTES;
    }
    else
    {
        note = &queue->notes[queue->writeIndex];
        queue->writeIndex = (queue->writeIndex + 1) % CONFIG_MAX_NOTES;
        queue->length++;
    }

    assert(note != NULL);

    memcpy(note->buffer, buffer, count);
    note->buffer[count] = '\0';
    note->length = count;
    note->sender = sender->id;
    return 0;
}

void note_handle_pending(interrupt_frame_t* frame, cpu_t* self)
{
    (void)frame;
    (void)self;

    thread_t* thread = sched_thread_unsafe();
    process_t* process = thread->process;
    note_queue_t* queue = &thread->notes;

    lock_acquire(&queue->lock);

    if (queue->flags & NOTE_QUEUE_RECIEVED_KILL)
    {
        lock_release(&queue->lock);
        process_kill(process, EXIT_FAILURE);
        atomic_store(&thread->state, THREAD_DYING);
        return;
    }

    while (true)
    {
        if (queue->length == 0)
        {
            lock_release(&queue->lock);
            return;
        }

        note_t* note = &queue->notes[queue->readIndex];
        queue->readIndex = (queue->readIndex + 1) % CONFIG_MAX_NOTES;
        queue->length--;

        LOG_WARN("unknown note '%.*s' received in thread tid=%d\n", note->length, note->buffer, thread->id);
        // TODO: Software interrupts.
    }
}
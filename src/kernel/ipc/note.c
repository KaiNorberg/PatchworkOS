#include <kernel/cpu/interrupt.h>
#include <kernel/ipc/note.h>

#include <kernel/cpu/cpu.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/space.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>
#include <kernel/sync/lock.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/math.h>

void note_handler_init(note_handler_t* handler)
{
    handler->func = NULL;
    lock_init(&handler->lock);
}

void note_queue_init(note_queue_t* queue)
{
    queue->readIndex = 0;
    queue->writeIndex = 0;
    queue->length = 0;
    queue->flags = NOTE_QUEUE_NONE;
    memset_s(&queue->noteFrame, sizeof(interrupt_frame_t), 0, sizeof(interrupt_frame_t));
    lock_init(&queue->lock);
}

uint64_t note_amount(note_queue_t* queue)
{
    LOCK_SCOPE(&queue->lock);
    uint64_t length = queue->length;
    if (queue->flags & NOTE_QUEUE_RECEIVED_KILL)
    {
        length++;
    }
    return length;
}

uint64_t note_send(note_queue_t* queue, const char* string)
{
    if (queue == NULL || string == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    size_t count = strnlen_s(string, NOTE_MAX);
    if (count == 0 || count >= NOTE_MAX)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* sender = process_current();

    LOCK_SCOPE(&queue->lock);

    if (strcmp(string, "kill") == 0)
    {
        queue->flags |= NOTE_QUEUE_RECEIVED_KILL;
        return 0;
    }

    if (queue->length >= CONFIG_MAX_NOTES)
    {
        errno = EAGAIN;
        return ERR;
    }

    note_t* note = &queue->notes[queue->writeIndex];
    queue->writeIndex = (queue->writeIndex + 1) % CONFIG_MAX_NOTES;
    queue->length++;

    memcpy_s(note->buffer, NOTE_MAX, string, count);
    note->buffer[count] = '\0';
    note->sender = sender->id;
    return 0;
}

bool note_handle_pending(interrupt_frame_t* frame)
{
    UNUSED(frame);

    if (!INTERRUPT_FRAME_IN_USER_SPACE(frame))
    {
        return false;
    }

    thread_t* thread = thread_current_unsafe();
    process_t* process = thread->process;
    note_queue_t* queue = &thread->notes;
    note_handler_t* handler = &process->noteHandler;

    LOCK_SCOPE(&queue->lock);

    if (queue->flags & NOTE_QUEUE_RECEIVED_KILL)
    {
        atomic_store(&thread->state, THREAD_DYING);
        queue->flags &= ~NOTE_QUEUE_RECEIVED_KILL;
        return true;
    }

    if (queue->length == 0 || (queue->flags & NOTE_QUEUE_HANDLING))
    {
        return false;
    }

    LOCK_SCOPE(&handler->lock);

    if (handler->func == NULL)
    {
        atomic_store(&thread->state, THREAD_DYING);
        return false;
    }

    note_t* note = &queue->notes[queue->readIndex];
    queue->readIndex = (queue->readIndex + 1) % CONFIG_MAX_NOTES;
    queue->length--;

    queue->noteFrame = *frame;
    queue->flags |= NOTE_QUEUE_HANDLING;

    // func(note->buffer)
    frame->rsp = ROUND_DOWN(frame->rsp - (RED_ZONE_SIZE + NOTE_MAX), 16);
    if (thread_copy_to_user(thread, (void*)frame->rsp, note->buffer, NOTE_MAX) == ERR)
    {
        atomic_store(&thread->state, THREAD_DYING);
        return true;
    }
    frame->rip = (uint64_t)handler->func;
    frame->rdi = frame->rsp;
    assert(frame->rflags & RFLAGS_INTERRUPT_ENABLE);

    LOG_DEBUG("delivering note '%s' to pid=%d rsp=%p rip=%p\n", note->buffer, process->id, (void*)frame->rsp,
        (void*)frame->rip);

    return true;
}

SYSCALL_DEFINE(SYS_NOTIFY, uint64_t, note_func_t handler)
{
    process_t* process = process_current();
    note_handler_t* noteHandler = &process->noteHandler;

    if (handler != NULL && space_check_access(&process->space, (void*)handler, 1) == ERR)
    {
        return ERR;
    }

    lock_acquire(&noteHandler->lock);
    noteHandler->func = handler;
    lock_release(&noteHandler->lock);

    return 0;
}

SYSCALL_DEFINE(SYS_NOTED, void)
{
    thread_t* thread = thread_current();
    note_queue_t* queue = &thread->notes;

    lock_acquire(&queue->lock);

    if (!(queue->flags & NOTE_QUEUE_HANDLING))
    {
        lock_release(&queue->lock);
        sched_thread_exit();
    }

    queue->flags &= ~NOTE_QUEUE_HANDLING;

    // Causes the system call to return to the saved interrupt frame.
    memcpy_s(thread->syscall.frame, sizeof(interrupt_frame_t), &queue->noteFrame, sizeof(interrupt_frame_t));
    thread->syscall.flags |= SYSCALL_FORCE_FAKE_INTERRUPT;

    lock_release(&queue->lock);
}
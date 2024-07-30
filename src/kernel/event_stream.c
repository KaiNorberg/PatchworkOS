#include "event_stream.h"

#include <stdlib.h>
#include <sys/math.h>

#include "lock.h"
#include "sched.h"
#include "sysfs.h"

static uint64_t event_stream_avail_count(event_stream_t* stream, uint64_t readIndex)
{
    uint64_t count = 0;
    while (readIndex != stream->writeIndex)
    {
        count++;
        readIndex = (readIndex + 1) % stream->length;
    }
    return count;
}

static uint64_t event_stream_read(file_t* file, void* buffer, uint64_t count)
{
    event_stream_t* stream = file->private;

    count = ROUND_DOWN(count, stream->eventSize);
    for (uint64_t i = 0; i < count / stream->eventSize; i++)
    {
        if (SCHED_BLOCK_LOCK(&stream->blocker, &stream->lock, file->pos != stream->writeIndex) != BLOCK_NORM)
        {
            lock_release(&stream->lock);
            return i * stream->eventSize;
        }

        memcpy((uint8_t*)buffer + stream->eventSize * i, (uint8_t*)stream->buffer + stream->eventSize * file->pos,
            stream->eventSize);
        file->pos = (file->pos + 1) % stream->length;

        lock_release(&stream->lock);
    }

    return count;
}

static uint64_t event_stream_status(file_t* file, poll_file_t* pollFile)
{
    event_stream_t* stream = file->private;
    pollFile->occurred = POLL_READ & (stream->writeIndex != file->pos);
    return 0;
}

static file_ops_t fileOps = {
    .read = event_stream_read,
    .status = event_stream_status,
};

static void event_stream_delete(void* private)
{
    event_stream_t* stream = private;
    free(stream->buffer);
    blocker_cleanup(&stream->blocker);
}

uint64_t event_stream_init(event_stream_t* stream, const char* path, const char* name, uint64_t eventSize, uint64_t length)
{
    stream->writeIndex = 0;
    stream->eventSize = eventSize;
    stream->length = length;
    stream->buffer = calloc(length, eventSize);
    stream->resource = sysfs_expose(path, name, &fileOps, stream, NULL, event_stream_delete);
    if (stream->resource == NULL)
    {
        return ERR;
    }
    blocker_init(&stream->blocker);
    lock_init(&stream->lock);

    return 0;
}

uint64_t event_stream_cleanup(event_stream_t* stream)
{
    return sysfs_hide(stream->resource);
}

uint64_t event_stream_push(event_stream_t* stream, const void* event, uint64_t eventSize)
{
    if (stream->eventSize != eventSize)
    {
        return ERROR(EINVAL);
    }
    LOCK_GUARD(&stream->lock);

    memcpy((void*)((uintptr_t)stream->buffer + stream->eventSize * stream->writeIndex), event, stream->eventSize);
    stream->writeIndex = (stream->writeIndex + 1) % stream->length;
    sched_unblock(&stream->blocker);

    return 0;
}

#include "input.h"

#include <stdlib.h>
#include <sys/math.h>

#include "lock.h"
#include "sysfs.h"

static uint64_t input_read(file_t* file, void* buffer, uint64_t count)
{
    input_t* input = file->internal;

    count = ROUND_DOWN(count, input->eventSize);
    for (uint64_t i = 0; i < count / input->eventSize; i++)
    {
        SCHED_WAIT(input->writeIndex != file->position, NEVER);
        LOCK_GUARD(&input->lock);

        if (file->position == input->writeIndex)
        {
            return i * input->eventSize;
        }

        memcpy((uint8_t*)buffer + input->eventSize * i, (uint8_t*)input->buffer + input->eventSize * file->position,
            input->eventSize);
        file->position = (file->position + 1) % input->length;
    }

    return count;
}

static uint64_t input_status(file_t* file, poll_file_t* pollFile)
{
    input_t* input = file->internal;
    pollFile->occurred = POLL_READ & (input->writeIndex != file->position);
    return 0;
}

static file_ops_t fileOps = {
    .read = input_read,
    .status = input_status,
};

static void input_delete(void* internal)
{
    input_t* input = internal;
    free(input->buffer);
}

uint64_t input_init(input_t* input, const char* path, const char* name, uint64_t eventSize, uint64_t length)
{
    input->writeIndex = 0;
    input->eventSize = eventSize;
    input->length = length;
    input->buffer = calloc(length, eventSize);
    input->resource = sysfs_expose(path, name, &fileOps, input, input_delete);
    if (input->resource == NULL)
    {
        return ERR;
    }
    lock_init(&input->lock);

    return 0;
}

uint64_t input_cleanup(input_t* input)
{
    return sysfs_hide(input->resource);
}

uint64_t input_push(input_t* input, const void* event, uint64_t eventSize)
{
    LOCK_GUARD(&input->lock);

    if (input->eventSize != eventSize)
    {
        return ERROR(EINVAL);
    }

    memcpy((void*)((uintptr_t)input->buffer + input->eventSize * input->writeIndex), event, input->eventSize);
    input->writeIndex = (input->writeIndex + 1) % input->length;
    return 0;
}

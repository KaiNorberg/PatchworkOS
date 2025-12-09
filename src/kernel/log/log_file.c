#include "gnu-efi/inc/x86_64/efibind.h"
#include <kernel/log/log_file.h>

#include <kernel/fs/file.h>
#include <kernel/fs/sysfs.h>
#include <kernel/log/log.h>
#include <kernel/log/log_screen.h>
#include <kernel/log/panic.h>
#include <kernel/sched/wait.h>
#include <kernel/utils/ring.h>

#include <stdlib.h>
#include <sys/io.h>

static lock_t lock = LOCK_CREATE();

static wait_queue_t waitQueue = WAIT_QUEUE_CREATE(waitQueue);

static char workingBuffer[LOG_FILE_MAX_BUFFER] = {0};
static char buffer[LOG_FILE_MAX_BUFFER] = {0};
static ring_t ring = RING_CREATE(buffer, sizeof(buffer));
static dentry_t* file = NULL;

static uint64_t log_file_op_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    (void)file; // Unused

    LOCK_SCOPE(&lock);
    uint64_t result = ring_read_at(&ring, *offset, buffer, count);
    *offset += result;
    return result;
}

static uint64_t log_file_op_write(file_t* file, const void* buffer, uint64_t count, uint64_t* offset)
{
    (void)file;   // Unused
    (void)offset; // Unused
    (void)count;  // Unused
    (void)buffer; // Unused

    // log_nprint(LOG_LEVEL_USER, buffer, count);
    return count;
}

static wait_queue_t* log_file_op_poll(file_t* file, poll_events_t* revents)
{
    (void)file;

    LOCK_SCOPE(&lock);
    if (ring.writeIndex != file->pos)
    {
        *revents |= POLLIN;
    }

    return &waitQueue;
}

static file_ops_t logFileOps = {
    .read = log_file_op_read,
    .write = log_file_op_write,
    .poll = log_file_op_poll,
};

void log_file_expose(void)
{
    file = sysfs_file_new(NULL, "klog", NULL, &logFileOps, NULL);
    if (file == NULL)
    {
        panic(NULL, "failed to create klog sysfs file");
    }
}

static void log_file_advance_fake_cursor(char chr, uint64_t* lineLength, uint64_t* lineCount)
{
    if (chr == '\n')
    {
        (*lineCount)++;
        *lineLength = 0;
    }
    else if (*lineLength >= log_screen_get_width())
    {
        (*lineCount)++;
        *lineLength = SCREEN_WRAP_INDENT;
    }
    else
    {
        (*lineLength)++;
    }
}

void log_file_flush_to_screen(void)
{
    LOCK_SCOPE(&lock);
    log_screen_clear();

    uint64_t lineLength = 0;
    uint64_t lineCount = 0;
    for (uint64_t i = 0; i < ring_data_length(&ring); i++)
    {
        uint8_t chr;
        ring_get_byte(&ring, i, &chr);

        log_file_advance_fake_cursor(chr, &lineLength, &lineCount);
    }

    uint64_t totalLines = lineCount;
    uint64_t linesThatFit = log_screen_get_height();

    lineLength = 0;
    lineCount = 0;

    uint64_t i = 0;
    for (; i < ring_data_length(&ring) && lineCount < totalLines - linesThatFit; i++)
    {
        uint8_t chr;
        ring_get_byte(&ring, i, &chr);

        log_file_advance_fake_cursor(chr, &lineLength, &lineCount);
    }

    for (; i < ring_data_length(&ring);)
    {
        uint64_t toRead = MIN(ring_data_length(&ring) - i, sizeof(workingBuffer));
        ring_read_at(&ring, i, workingBuffer, toRead);
        i += toRead;

        log_screen_write(workingBuffer, toRead);
    }
}

void log_file_write(const char* string, uint64_t length)
{
    if (string == NULL || length == 0)
    {
        return;
    }

    LOCK_SCOPE(&lock);
    ring_write(&ring, string, length);

    wait_unblock(&waitQueue, WAIT_ALL, EOK);
}

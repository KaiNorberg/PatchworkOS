#include <kernel/fs/devfs.h>
#include <kernel/fs/file.h>
#include <kernel/log/log.h>

#include <kernel/cpu/cpu.h>
#include <kernel/drivers/com.h>
#include <kernel/init/boot_info.h>
#include <kernel/log/screen.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>

#include <boot/boot_info.h>
#include <kernel/version.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/io.h>
#include <sys/math.h>
#include <sys/proc.h>

static lock_t lock = LOCK_CREATE();

static char klogBuffer[CONFIG_KLOG_SIZE];
static uint64_t klogHead = 0;
static dentry_t* klog = NULL;

static char lineBuffer[LOG_MAX_BUFFER] = {0};
static char workingBuffer[LOG_MAX_BUFFER] = {0};
static bool isLastCharNewline = false;
static bool firstHeaderPrinted = false;

static const char* levelNames[] = {
    [LOG_LEVEL_DEBUG] = "D",
    [LOG_LEVEL_USER] = "U",
    [LOG_LEVEL_INFO] = "I",
    [LOG_LEVEL_WARN] = "W",
    [LOG_LEVEL_ERR] = "E",
    [LOG_LEVEL_PANIC] = "P",
};

static size_t klog_read(file_t* file, void* buffer, size_t count, size_t* offset)
{
    UNUSED(file);

    LOCK_SCOPE(&lock);

    if (*offset >= klogHead)
    {
        return 0;
    }

    for (size_t i = 0; i < count; i++)
    {
        if (*offset >= klogHead)
        {
            return i;
        }

        ((char*)buffer)[i] = klogBuffer[(*offset)++ % CONFIG_KLOG_SIZE];
    }

    return count;
}

static size_t klog_write(file_t* file, const void* buffer, size_t count, size_t* offset)
{
    UNUSED(file);
    UNUSED(offset);

    log_nprint(LOG_LEVEL_INFO, buffer, count);
    return count;
}

static file_ops_t klogOps = {
    .read = klog_read,
    .write = klog_write,
};

static void log_splash(void)
{
#ifdef NDEBUG
    LOG_INFO("Booting %s-kernel %s (Built %s %s)\n", OS_NAME, OS_VERSION, __DATE__, __TIME__);
#else
    LOG_INFO("Booting %s-kernel DEBUG %s (Built %s %s)\n", OS_NAME, OS_VERSION, __DATE__, __TIME__);
#endif
    LOG_INFO("Copyright (C) 2025 Kai Norberg. MIT Licensed.\n");
}

void log_init(void)
{
    const boot_info_t* bootInfo = boot_info_get();
    assert(bootInfo != NULL);

    const boot_gop_t* gop = &bootInfo->gop;
    assert(gop->virtAddr != NULL);

    isLastCharNewline = true;

    screen_init();

#if CONFIG_LOG_SERIAL
    com_init(COM1);
#endif

    log_splash();
}

void log_expose(void)
{
    LOCK_SCOPE(&lock);

    if (klog != NULL)
    {
        return;
    }

    klog = devfs_file_new(NULL, "klog", NULL, &klogOps, NULL);
    if (klog == NULL)
    {
        return;
    }
}

static void log_write(const char* string, uint64_t length)
{
    for (uint64_t i = 0; i < length; i++)
    {
        klogBuffer[klogHead++ % CONFIG_KLOG_SIZE] = string[i];
    }

#if CONFIG_LOG_SERIAL
    for (uint64_t i = 0; i < length; i++)
    {
        com_write(COM1, string[i]);
    }
#endif

    screen_write(string, length);
}

static void log_print_header(log_level_t level)
{
    if (!firstHeaderPrinted)
    {
        firstHeaderPrinted = true;
    }
    else
    {
        log_write("\n", 1);
    }

    if (level == LOG_LEVEL_PANIC)
    {
        int length = snprintf(workingBuffer, sizeof(workingBuffer), "[XXXX.XXX-XX-X] ");
        log_write(workingBuffer, length);
        return;
    }

    clock_t uptime = clock_uptime();
    uint64_t seconds = uptime / CLOCKS_PER_SEC;
    uint64_t milliseconds = (uptime % CLOCKS_PER_SEC) / (CLOCKS_PER_MS);

    int length = snprintf(workingBuffer, sizeof(workingBuffer), "[%4llu.%03llu-%02x-%s] ", seconds, milliseconds,
        SELF->id, levelNames[level]);
    log_write(workingBuffer, length);
}

static void log_handle_char(log_level_t level, char chr)
{
    if (isLastCharNewline && chr != '\n')
    {
        isLastCharNewline = false;

        log_print_header(level);
    }

    if (chr == '\n')
    {
        if (isLastCharNewline)
        {
            log_print_header(level);
        }
        isLastCharNewline = true;
        return;
    }

    log_write(&chr, 1);
}

void log_nprint(log_level_t level, const char* string, uint64_t length)
{
    if (level != LOG_LEVEL_PANIC)
    {
        lock_acquire(&lock);
    }

    for (uint64_t i = 0; i < length; i++)
    {
        log_handle_char(level, string[i]);
    }

    if (level != LOG_LEVEL_PANIC)
    {
        lock_release(&lock);
    }
}

void log_print(log_level_t level, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_vprint(level, format, args);
    va_end(args);
}

void log_vprint(log_level_t level, const char* format, va_list args)
{
    if (level != LOG_LEVEL_PANIC)
    {
        lock_acquire(&lock);
    }

    int length = vsnprintf(lineBuffer, LOG_MAX_BUFFER, format, args);
    assert(length >= 0);

    if (length >= LOG_MAX_BUFFER)
    {
        length = LOG_MAX_BUFFER - 1;
        lineBuffer[length] = '\0';
    }

    for (int i = 0; i < length; i++)
    {
        log_handle_char(level, lineBuffer[i]);
    }

    if (level != LOG_LEVEL_PANIC)
    {
        lock_release(&lock);
    }
}

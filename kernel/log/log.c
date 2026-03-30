#include <kernel/fs/file.h>
#include <kernel/fs/sysfs.h>
#include <kernel/io/irp.h>
#include <kernel/log/log.h>

#include <kernel/cpu/cpu.h>
#include <kernel/drivers/com.h>
#include <kernel/log/screen.h>
#include <kernel/proc/process.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/start/boot_info.h>
#include <kernel/sync/lock.h>

#include <boot/boot_info.h>
#include <kernel/version.h>

#include <libc/fs.h>
#include <libc/math.h>
#include <libc/proc.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static lock_t lock = LOCK_CREATE();

static char klogBuffer[CONFIG_KLOG_SIZE];
static size_t klogHead = 0;
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

static void log_handle_char(log_level_t level, char chr);

static status_t klog_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);

    LOCK_SCOPE(&lock);

    /// @todo Reimplement this.
    return ERR(DRIVER, IMPL);
}

static status_t klog_write(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);

    lock_acquire(&lock);

    size_t bytesWritten = 0;
    uint8_t* c;
    SGLIST_FOR_EACH(c, frame->write.buffer)
    {
        log_handle_char(LOG_LEVEL_USER, (char)*c);
        bytesWritten++;
    }

    lock_release(&lock);

    irp->result = bytesWritten;
    return OK;
}

static vnode_class_t klogClass = {
    .name = "klog",
    .type = FILE_TYPE_DEVICE,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_READ] = klog_read,
            [IRP_MJ_WRITE] = klog_write,
        },
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

dentry_t* log_dentry(void)
{
    return REF(klog);
}

void log_expose(void)
{
    LOCK_SCOPE(&lock);

    if (klog != NULL)
    {
        return;
    }

    klog = sysfs_dentry_new(NULL, "klog", &klogClass, NULL);
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

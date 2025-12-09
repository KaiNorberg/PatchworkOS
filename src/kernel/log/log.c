#include <kernel/log/log.h>

#include <kernel/cpu/cpu.h>
#include <kernel/drivers/com.h>
#include <kernel/log/log_file.h>
#include <kernel/log/log_screen.h>
#include <kernel/sched/sys_time.h>
#include <kernel/sched/timer.h>
#include <kernel/sync/lock.h>
#include <kernel/init/boot_info.h>

#include <boot/boot_info.h>
#include <kernel/version.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/io.h>
#include <sys/proc.h>

static lock_t lock = LOCK_CREATE();

static char lineBuffer[LOG_MAX_BUFFER] = {0};
static char workingBuffer[LOG_MAX_BUFFER] = {0};
static log_output_t outputs = 0;
static log_level_t minLevel = 0;
static bool isLastCharNewline = 0;
static bool firstHeaderPrinted = false;

static const char* levelNames[] = {
    [LOG_LEVEL_DEBUG] = "D",
    [LOG_LEVEL_USER] = "U",
    [LOG_LEVEL_INFO] = "I",
    [LOG_LEVEL_WARN] = "W",
    [LOG_LEVEL_ERR] = "E",
    [LOG_LEVEL_PANIC] = "P",
};

static void log_splash(void)
{
#ifdef NDEBUG
    LOG_INFO("Booting %s-kernel %s (Built %s %s)\n", OS_NAME, OS_VERSION, __DATE__, __TIME__);
#else
    LOG_INFO("Booting %s-kernel DEBUG %s (Built %s %s)\n", OS_NAME, OS_VERSION, __DATE__, __TIME__);
#endif
    LOG_INFO("Copyright (C) 2025 Kai Norberg. MIT Licensed. See /usr/license/LICENSE for details.\n");
    LOG_INFO("min_level=%s outputs=%s%s%s\n", levelNames[minLevel], (outputs & LOG_OUTPUT_SERIAL) ? "serial " : "",
        (outputs & LOG_OUTPUT_SCREEN) ? "screen " : "", (outputs & LOG_OUTPUT_FILE) ? "file " : "");
}

void log_init(void)
{
    const boot_info_t* bootInfo = boot_info_get();
    const boot_gop_t* gop = &bootInfo->gop;

    log_screen_init(gop);

    outputs = 0;
#ifndef NDEBUG
    minLevel = LOG_LEVEL_DEBUG;
#else
    minLevel = LOG_LEVEL_USER;
#endif
    isLastCharNewline = true;

    outputs |= LOG_OUTPUT_SCREEN;
    outputs |= LOG_OUTPUT_FILE;
#if CONFIG_LOG_SERIAL
    com_init(COM1);
    outputs |= LOG_OUTPUT_SERIAL;
#endif

    log_splash();
}

void log_screen_enable()
{
    LOCK_SCOPE(&lock);

    if (outputs & LOG_OUTPUT_SCREEN)
    {
        return;
    }

    log_file_flush_to_screen();
    outputs |= LOG_OUTPUT_SCREEN;
}

void log_screen_disable(void)
{
    LOCK_SCOPE(&lock);

    outputs &= ~LOG_OUTPUT_SCREEN;
}

static void log_write(const char* string, uint64_t length)
{
    if (outputs & LOG_OUTPUT_FILE)
    {
        log_file_write(string, length);
    }

    if (outputs & LOG_OUTPUT_SERIAL)
    {
        for (uint64_t i = 0; i < length; i++)
        {
            com_write(COM1, string[i]);
        }
    }

    if (outputs & LOG_OUTPUT_SCREEN)
    {
        log_screen_write(string, length);
    }
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

    clock_t uptime = sys_time_uptime();
    uint64_t seconds = uptime / CLOCKS_PER_SEC;
    uint64_t milliseconds = (uptime % CLOCKS_PER_SEC) / (CLOCKS_PER_SEC / 1000);

    cpu_t* self = cpu_get_unsafe();

    int length = snprintf(workingBuffer, sizeof(workingBuffer), "[%4llu.%03llu-%02x-%s] ", seconds, milliseconds,
        self->id, levelNames[level]);
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
    if (level < minLevel)
    {
        return;
    }

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
    if (level < minLevel)
    {
        return;
    }

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

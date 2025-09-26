#include "log.h"

#include "cpu/smp.h"
#include "drivers/com.h"
#include "fs/file.h"
#include "log/panic.h"
#include "sched/timer.h"
#include "sync/lock.h"
#include "utils/ring.h"

#include <_internal/MAX_PATH.h>
#include <boot/boot_info.h>
#include <common/version.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/io.h>
#include <sys/proc.h>

static log_state_t state = {0};
static log_file_t klog = {0};
static screen_t screen = {0};

static lock_t lock;

static const char* levelNames[] = {
    [LOG_LEVEL_DEBUG] = "D",
    [LOG_LEVEL_USER] = "U",
    [LOG_LEVEL_INFO] = "I",
    [LOG_LEVEL_WARN] = "W",
    [LOG_LEVEL_ERR] = "E",
    [LOG_LEVEL_PANIC] = "P",
};

static void log_splash_screen(void)
{
#ifdef NDEBUG
    LOG_INFO("Booting %s-kernel %s (Built %s %s)\n", OS_NAME, OS_VERSION, __DATE__, __TIME__);
#else
    LOG_INFO("Booting %s-kernel DEBUG %s (Built %s %s)\n", OS_NAME, OS_VERSION, __DATE__, __TIME__);
#endif
    LOG_INFO("Copyright (C) 2025 Kai Norberg. MIT Licensed. See /usr/license/LICENSE for details.\n");
    LOG_INFO("min_level=%s outputs=%s%s%s\n", levelNames[state.config.minLevel],
        (state.config.outputs & LOG_OUTPUT_SERIAL) ? "serial " : "",
        (state.config.outputs & LOG_OUTPUT_SCREEN) ? "screen " : "",
        (state.config.outputs & LOG_OUTPUT_FILE) ? "file " : "");
}

void log_init(boot_gop_t* gop)
{
    lock_init(&lock);

    state.config.outputs = 0;
#ifndef NDEBUG
    state.config.minLevel = LOG_LEVEL_DEBUG;
#else
    state.config.minLevel = LOG_LEVEL_USER;
#endif
    atomic_init(&state.panickingCpuId, LOG_NO_PANIC_CPU_ID);
    state.isLastCharNewline = true;

    screen_init(&screen, gop);
    state.config.outputs |= LOG_OUTPUT_SCREEN;

    ring_init(&klog.ring, klog.buffer, LOG_MAX_BUFFER);
    state.config.outputs |= LOG_OUTPUT_FILE;

#if CONFIG_LOG_SERIAL
    com_init(COM1);
    state.config.outputs |= LOG_OUTPUT_SERIAL;
#endif

    log_splash_screen();
}

void log_screen_enable()
{
    LOG_INFO("screen enable\n");
    LOCK_SCOPE(&lock);

    screen_enable(&screen, &klog.ring);
    state.config.outputs |= LOG_OUTPUT_SCREEN;
}

void log_disable_screen(void)
{
    LOCK_SCOPE(&lock);

    screen_disable(&screen);
    state.config.outputs &= ~LOG_OUTPUT_SCREEN;
}

screen_t* log_get_screen(void)
{
    return &screen;
}

log_state_t* log_get_state(void)
{
    return &state;
}

static uint64_t klog_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    (void)file; // Unused

    LOCK_SCOPE(&lock);

    uint64_t result = ring_read_at(&klog.ring, *offset, buffer, count);
    *offset += result;
    return result;
}

static uint64_t klog_write(file_t* file, const void* buffer, uint64_t count, uint64_t* offset)
{
    (void)file; // Unused

    if (count == 0 || buffer == NULL || offset == NULL)
    {
        return 0;
    }

    if (count > MAX_PATH)
    {
        errno = EINVAL;
        return ERR;
    }

    char string[MAX_PATH];
    memcpy(string, buffer, count);
    string[count] = '\0';

    log_print(LOG_LEVEL_USER, "user_space", string);
    *offset += count;
    return count;
}

static file_ops_t klogOps = {
    .read = klog_read,
    .write = klog_write,
};

void log_file_expose(void)
{
    LOCK_SCOPE(&lock);

    if (sysfs_file_init(&klog.file, sysfs_get_default(), "klog", NULL, &klogOps, NULL) == ERR)
    {
        panic(NULL, "Failed to expose log file");
    }
}

void log_write(const char* string, uint64_t length)
{
    if (state.config.outputs & LOG_OUTPUT_FILE)
    {
        ring_write(&klog.ring, string, length);
    }

    if (state.config.outputs & LOG_OUTPUT_SERIAL)
    {
        for (uint64_t i = 0; i < length; i++)
        {
            com_write(COM1, string[i]);
        }
    }

    if (state.config.outputs & LOG_OUTPUT_SCREEN)
    {
        screen_write(&screen, string, length);
    }
}

static void log_print_header(log_level_t level, const char* prefix)
{
    char buffer[MAX_PATH];

    uint64_t uptime = timer_uptime();
    uint64_t seconds = uptime / CLOCKS_PER_SEC;
    uint64_t milliseconds = (uptime % CLOCKS_PER_SEC) / (CLOCKS_PER_SEC / 1000);

    cpu_t* self = smp_self_unsafe();

    int length = sprintf(buffer, "[%4llu.%03llu-%02x-%s-%-10s] ", seconds, milliseconds, self->id, levelNames[level],
        prefix != NULL ? prefix : "unknown");
    log_write(buffer, length);
}

static void log_handle_char(log_level_t level, const char* prefix, char chr)
{
    if (state.isLastCharNewline && chr != '\n')
    {
        state.isLastCharNewline = false;

        log_print_header(level, prefix);
    }

    if (chr == '\n')
    {
        if (state.isLastCharNewline)
        {
            log_print_header(level, prefix);
        }
        state.isLastCharNewline = true;
    }

    log_write(&chr, 1);
}

uint64_t log_print(log_level_t level, const char* prefix, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    uint64_t result = log_vprint(level, prefix, format, args);
    va_end(args);
    return result;
}

uint64_t log_vprint(log_level_t level, const char* prefix, const char* format, va_list args)
{
    LOCK_SCOPE(&lock);

    if (level < state.config.minLevel)
    {
        return 0;
    }

    int length = vsnprintf(state.lineBuffer, LOG_MAX_BUFFER, format, args);
    if (length < 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (length >= LOG_MAX_BUFFER)
    {
        length = LOG_MAX_BUFFER - 1;
        state.lineBuffer[length] = '\0';
    }

    for (int i = 0; i < length; i++)
    {
        log_handle_char(level, prefix, state.lineBuffer[i]);
    }

    return 0;
}

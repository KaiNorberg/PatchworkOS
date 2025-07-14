#include "log.h"

#include "cpu/smp.h"
#include "drivers/com.h"
#include "drivers/systime/systime.h"
#include "log/panic.h"
#include "mem/pmm.h"
#include "sched/thread.h"
#include "sync/lock.h"
#include "utils/ring.h"

#include <boot/boot_info.h>
#include <common/version.h>

#include <assert.h>
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
    [LOG_LEVEL_DEBUG] = "DBUG",
    [LOG_LEVEL_USER] = "USER",
    [LOG_LEVEL_INFO] = "INFO",
    [LOG_LEVEL_WARN] = "WARN",
    [LOG_LEVEL_ERR] = "EROR",
    [LOG_LEVEL_PANIC] = "PANC",
};

void log_init(void)
{
    lock_init(&lock);

    atomic_init(&state.panickingCpuId, LOG_NO_PANIC_CPU_ID);
    state.config.isTimeEnabled = false;

#if CONFIG_LOG_SERIAL
    state.config.outputs = LOG_OUTPUT_SERIAL;
#else
    state.config.outputs = 0;
#endif

#ifndef NDEBUG
    state.config.minLevel = LOG_LEVEL_DEBUG;
#else
    state.config.minLevel = LOG_LEVEL_USER;
#endif

    state.isLastCharNewline = true;

    ring_init(&klog.ring, klog.buffer, LOG_MAX_BUFFER);
    state.config.outputs |= LOG_OUTPUT_FILE;

#if CONFIG_LOG_SERIAL
    com_init(COM1);
#endif

#ifndef NDEBUG
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

void log_enable_time(void)
{
    LOCK_SCOPE(&lock);

    state.config.isTimeEnabled = true;
}

void log_screen_enable(gop_buffer_t* framebuffer)
{
    LOG_INFO("screen enable\n");
    LOCK_SCOPE(&lock);

    if (!screen.initialized)
    {
        if (screen_init(&screen, framebuffer) == ERR)
        {
            panic(NULL, "Failed to initialize screen");
        }
    }

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
    LOCK_SCOPE(&lock);

    uint64_t result = ring_read_at(&klog.ring, *offset, buffer, count);
    *offset += result;
    return result;
}

static uint64_t klog_write(file_t* file, const void* buffer, uint64_t count, uint64_t* offset)
{
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

    const char* prefixEnd = strchr(string, ':');
    if (prefixEnd == NULL)
    {
        log_print(LOG_LEVEL_USER, "user_space", string);
        *offset += count;
        return count;
    }

    uint64_t prefixLen = prefixEnd - string;
    if (prefixLen >= MAX_PATH - 1)
    {
        errno = EINVAL;
        return ERR;
    }

    char prefix[MAX_PATH];
    memcpy(prefix, string, prefixLen);
    prefix[prefixLen] = '\0';

    log_print(LOG_LEVEL_USER, prefix, string);
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
    if (level == LOG_LEVEL_PANIC)
    {
        return;
    }

    uint64_t uptime = state.config.isTimeEnabled ? systime_uptime() : 0;

    uint64_t seconds = uptime / CLOCKS_PER_SEC;
    uint64_t milliseconds = (uptime % CLOCKS_PER_SEC) / (CLOCKS_PER_SEC / 1000);

    cpu_t* self = smp_self_unsafe();

    int length = sprintf(state.timestampBuffer, "[%5llu.%03llu-%03d-%s-%-13s] ", seconds, milliseconds, self->id,
        levelNames[level], prefix != NULL ? prefix : "unknown");

    log_write(state.timestampBuffer, length);
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

#pragma once

#include "vfs.h"

// Note: View files are files that provide a view of system state, for instace sys:/proc/cwd, or sys:/stat/cpu

#define VIEW_STANDARD_OPS_DEFINE(name, ...) \
    static view_ops_t name##ops = __VA_ARGS__; \
    static uint64_t name##read(file_t* file, void* buffer, uint64_t count) \
    { \
        view_t view; \
        if (name##ops.init(file, &view) == ERR) \
        { \
            return ERR; \
        } \
        uint64_t result = BUFFER_READ(file, buffer, count, view.buffer, view.length); \
        if (name##ops.deinit != NULL) \
        { \
            name##ops.deinit(&view); \
        } \
        return result; \
    } \
    static uint64_t name##seek(file_t* file, int64_t offset, seek_origin_t origin) \
    { \
        view_t view; \
        if (name##ops.init(file, &view) == ERR) \
        { \
            return ERR; \
        } \
        uint64_t result = BUFFER_SEEK(file, offset, origin, view.length); \
        if (name##ops.deinit != NULL) \
        { \
            name##ops.deinit(&view); \
        } \
        return result; \
    } \
    SYSFS_STANDARD_OPS_DEFINE(name, \
        (file_ops_t){ \
            .read = name##read, \
            .seek = name##seek, \
        })

typedef struct view view_t;

typedef uint64_t (*view_init_t)(file_t* file, view_t*);
typedef void (*view_deinit_t)(view_t*);

typedef struct view_ops
{
    view_init_t init;
    view_deinit_t deinit;
} view_ops_t;

typedef struct view
{
    uint64_t length;
    void* buffer;
    void* private;
} view_t;
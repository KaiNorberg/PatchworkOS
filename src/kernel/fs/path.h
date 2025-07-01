#pragma once

#include "utils/map.h"

#include <stdint.h>
#include <sys/io.h>

typedef struct path path_t;
typedef struct mount mount_t;
typedef struct dentry dentry_t;

#define PATH_DEFER(path) __attribute__((cleanup(path_defer_cleanup))) path_t* CONCAT(i, __COUNTER__) = (path)

#define PATH_VALID_CHAR(ch) (isalnum((ch)) || strchr("_-. ()[]{}~!@#$%^&',;=+", (ch)))

#define PATH_HANDLE_DOTDOT_MAX_ITER 1000

typedef enum
{
    PATH_NONE = 0,
    PATH_NONBLOCK = 1 << 0,
    PATH_APPEND = 1 << 1,
    PATH_CREATE = 1 << 2,
    PATH_EXCLUSIVE = 1 << 3,
    PATH_TRUNCATE = 1 << 4,
    PATH_DIRECTORY = 1 << 5
} path_flags_t;

typedef struct path_flag_entry
{
    map_entry_t entry;
    path_flags_t flag;
    const char* name;
} path_flag_entry_t;

typedef struct path
{
    mount_t* mount;
    dentry_t* dentry;
} path_t;

typedef struct parsed_pathname
{
    char pathname[MAX_PATH];
    path_flags_t flags;
} parsed_pathname_t;

void path_flags_init(void);

uint64_t path_parse_pathname(parsed_pathname_t* dest, const char* pathname);

uint64_t path_walk(path_t* outPath, const char* pathname, const path_t* start);
uint64_t path_walk_parent(path_t* outPath, const char* pathname, const path_t* start, char* outLastName);

void path_copy(path_t* dest, const path_t* src);

void path_put(path_t* path);

static inline void path_defer_cleanup(path_t** path)
{
    if (*path != NULL)
    {
        path_put(*path);
    }
}
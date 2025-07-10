#pragma once

#include "utils/map.h"

#include <alloca.h>
#include <ctype.h>
#include <stdbool.h>
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

typedef struct pathname
{
    char string[MAX_PATH];
    path_flags_t flags;
    bool isValid;
} pathname_t;

void path_flags_init(void);

/**
 * @brief Helper to create a pathname.
 * @ingroup kernel_vfs
 *
 * The `PATHNAME()` macro is a helper to create a pathname from a string. Error handling is done via the
 * pathname::isValid flag. Since `pathname_init()` returns error codes its best to use it instead of this macro for
 * systems that return error codes to user space.
 *
 */
#define PATHNAME(string) \
    ({ \
        pathname_t* pathname = alloca(sizeof(pathname_t)); \
        pathname_init(pathname, string); \
        pathname; \
    })

uint64_t pathname_init(pathname_t* pathname, const char* string);

#define PATH_EMPTY \
    (path_t) \
    { \
        .mount = NULL, .dentry = NULL \
    }

#define PATH_CREATE(inMount, inDentry) \
    (path_t) \
    { \
        .mount = mount_ref(inMount), .dentry = dentry_ref(inDentry), \
    }

void path_set(path_t* path, mount_t* mount, dentry_t* dentry);
void path_copy(path_t* dest, const path_t* src);

void path_put(path_t* path);

uint64_t path_traverse(path_t* outPath, const path_t* parent, const char* name);

uint64_t path_walk(path_t* outPath, const pathname_t* pathname, const path_t* start);
uint64_t path_walk_parent(path_t* outPath, const pathname_t* pathname, const path_t* start, char* outLastName);

uint64_t path_to_name(const path_t* path, pathname_t* pathname);

static inline void path_defer_cleanup(path_t** path)
{
    if (*path != NULL)
    {
        path_put(*path);
    }
}

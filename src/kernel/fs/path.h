#pragma once

#include <alloca.h>
#include <common/node.h>
#include <stdint.h>
#include <sys/io.h>

#define PATH_NAME_SEPARATOR '/'
#define PATH_LABEL_SEPARATOR ':'
#define PATH_FLAGS_SEPARATOR '?'
#define PATH_FLAG_SEPARATOR '&'

#define PATH_VALID_CHAR(ch) (isalnum((ch)) || strchr("_-. ()[]{}~!@#$%^&',;=+", (ch)))

#define PATH_END_OF_NAME(ch) ((ch) == PATH_NAME_SEPARATOR || (ch) == PATH_FLAGS_SEPARATOR || (ch) == '\0')
#define PATH_END_OF_FLAG(ch) ((ch) == PATH_FLAG_SEPARATOR || (ch) == '\0')
#define PATH_END_OF_LABEL(ch) ((ch) == PATH_LABEL_SEPARATOR || (ch) == '\0')

#define PATH_NAME_IS_DOT(name) ((name)[0] == '.' && PATH_END_OF_NAME((name)[1]))
#define PATH_NAME_IS_DOT_DOT(name) ((name)[0] == '.' && (name)[1] == '.' && PATH_END_OF_NAME((name)[2]))

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

typedef struct path
{
    char volume[MAX_NAME];     // Just a string
    char buffer[MAX_PATH + 1]; // [name]\0[name]\0...\0\3
    uint64_t bufferLength;     // Includes \0 but not \3 chars.
    path_flags_t flags;
    bool isInvalid;
} path_t;

#define PATH_FOR_EACH(name, path) \
    for ((name) = (path)->buffer; (name)[0] != '\3' && (name)[1] != '\3'; (name) += strlen((name)) + 1)

// Used to more easily create paths from strings, yes the alloca call is a bit disgusting but since we are allocating a
// fixed size it is not a problem. Note that if path_init() fails, the path->isInvalid variable is set. It is very
// important to note that the lifetime of the returned path variable is limited to the calling function.
#define PATH(process, string) \
    ({ \
        path_t* p = alloca(sizeof(path_t)); \
        vfs_ctx_t* ctx = &process->vfsCtx; \
        LOCK_DEFER(&ctx->lock); \
        path_init(p, string, &ctx->cwd); \
        p; \
    })

uint64_t path_init(path_t* path, const char* string, path_t* cwd);

void path_to_string(const path_t* path, char* dest);

node_t* path_traverse_node(const path_t* path, node_t* node);

node_t* path_traverse_node_parent(const path_t* path, node_t* node);

bool path_is_name_valid(const char* name);

char* path_last_name(const path_t* path);

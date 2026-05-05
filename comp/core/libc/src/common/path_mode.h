#pragma once

#include <libc/fs.h>
#include <stdint.h>

typedef struct path_flag_short
{
    path_mode_t mode;
} path_flag_short_t;

static path_flag_short_t shortFlags[UINT8_MAX + 1] = {
    ['r'] = {.mode = PATH_MODE_READ},
    ['w'] = {.mode = PATH_MODE_WRITE},
    ['x'] = {.mode = PATH_MODE_EXECUTE},
    ['a'] = {.mode = PATH_MODE_APPEND},
    ['c'] = {.mode = PATH_MODE_CREATE},
    ['d'] = {.mode = PATH_MODE_DIRECTORY | PATH_MODE_CREATE},
    ['s'] = {.mode = PATH_MODE_SYMLINK | PATH_MODE_CREATE},
    ['h'] = {.mode = PATH_MODE_HARDLINK | PATH_MODE_CREATE},
    ['e'] = {.mode = PATH_MODE_EXCLUSIVE},
    ['E'] = {.mode = PATH_MODE_EXISTING},
    ['t'] = {.mode = PATH_MODE_TRUNCATE},
    ['l'] = {.mode = PATH_MODE_NOFOLLOW},
    ['p'] = {.mode = PATH_MODE_PARENTS},
    ['L'] = {.mode = PATH_MODE_LOCKED},
};

typedef struct path_flag
{
    path_mode_t mode;
    const char* name;
} path_flag_t;

static const path_flag_t flags[] = {
    {.mode = PATH_MODE_READ, .name = "read"},
    {.mode = PATH_MODE_WRITE, .name = "write"},
    {.mode = PATH_MODE_EXECUTE, .name = "execute"},
    {.mode = PATH_MODE_APPEND, .name = "append"},
    {.mode = PATH_MODE_CREATE, .name = "create"},
    {.mode = PATH_MODE_DIRECTORY | PATH_MODE_CREATE, .name = "directory"},
    {.mode = PATH_MODE_SYMLINK | PATH_MODE_CREATE, .name = "symlink"},
    {.mode = PATH_MODE_HARDLINK | PATH_MODE_CREATE, .name = "hardlink"},
    {.mode = PATH_MODE_EXCLUSIVE, .name = "exclusive"},
    {.mode = PATH_MODE_EXISTING, .name = "existing"},
    {.mode = PATH_MODE_TRUNCATE, .name = "truncate"},
    {.mode = PATH_MODE_NOFOLLOW, .name = "nofollow"},
    {.mode = PATH_MODE_PARENTS, .name = "parents"},
    {.mode = PATH_MODE_LOCKED, .name = "locked"},
};
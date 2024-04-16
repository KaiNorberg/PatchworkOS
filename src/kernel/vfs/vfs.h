#pragma once

#include <sys/io.h>

#include "defs/defs.h"
#include "lock/lock.h"

//NOTE: Only supports virtual disks for now.

typedef struct Filesystem Filesystem;
typedef struct Volume Volume;
typedef struct File File;

typedef struct Filesystem
{
    char* name;
    Volume* (*mount)(Filesystem*);
} Filesystem;

void vfs_init();

File* vfs_open(const char* path);

uint64_t vfs_realpath(char* out, const char* path);

uint64_t vfs_mount(char letter, Filesystem* fs);

uint64_t vfs_chdir(const char* path);
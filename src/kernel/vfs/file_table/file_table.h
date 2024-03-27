#pragma once

#include <stdatomic.h>
#include <sys/io.h>

#include "defs/defs.h"
#include "lock/lock.h"

#define FILE_TABLE_LENGTH 64

typedef struct Disk Disk;

typedef struct
{
    Disk* disk;
    void* context;
    uint8_t flags;
    _Atomic uint64_t position;
    _Atomic uint64_t ref;
} File;

typedef struct
{
    File* files[FILE_TABLE_LENGTH];
    Lock lock;
} FileTable;

void file_table_init(FileTable* table);

void file_table_cleanup(FileTable* table);

fd_t file_table_open(Disk* disk, uint8_t flags, void* context);

uint64_t file_table_close(fd_t fd);

File* file_table_get(fd_t fd);

void file_table_put(File* file);
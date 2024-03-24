#pragma once

#include <stdatomic.h>

#include "vfs/vfs.h"
#include "lock/lock.h"

#define FILE_TABLE_LENGTH 64

typedef struct
{
    //File* files[FILE_TABLE_LENGTH];
    Lock lock;
} FileTable;

void file_table_init(FileTable* table);

void file_table_cleanup(FileTable* table);

/*uint64_t file_table_open(Disk* disk, uint64_t flags, void* context);

uint64_t file_table_close(uint64_t fd);

File* file_table_get(uint64_t fd);

void file_table_put(File* file);*/
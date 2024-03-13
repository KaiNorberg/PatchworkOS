#pragma once

#include <stdint.h>

#include <lib-asym.h>

#include "vfs/vfs.h"

#define FILE_TABLE_LENGTH 64

typedef struct
{
    File* files[FILE_TABLE_LENGTH];
} FileTable;

FileTable* file_table_new(void);

void file_table_free(FileTable* fileTable);

Status file_table_open(FileTable* fileTable, uint64_t* out, const char* path, uint64_t flags);

File* file_table_get(FileTable* fileTable, uint64_t fd);

Status file_table_close(FileTable* fileTable, uint64_t fd);
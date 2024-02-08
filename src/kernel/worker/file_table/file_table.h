#pragma once

#include "vfs/vfs.h"

#include <lib-asym.h>

#define FILE_TABLE_LENGTH 1024

typedef struct
{
    File* files[FILE_TABLE_LENGTH];
} FileTable;

FileTable* file_table_new();

Status file_table_open(FileTable* fileTable, uint64_t* out, const char* path, uint64_t flags);

File* file_table_get(FileTable* fileTable, uint64_t fd);

Status file_table_close(FileTable* fileTable, uint64_t fd);

void file_table_free(FileTable* fileTable);
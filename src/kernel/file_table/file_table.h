#pragma once

#include <stdint.h>

#include <lib-system.h>

#include "vfs/vfs.h"

#define FILE_TABLE_LENGTH 64

typedef struct
{
    File* files[FILE_TABLE_LENGTH];
} FileTable;

FileTable* file_table_new(void);

void file_table_free(FileTable* fileTable);
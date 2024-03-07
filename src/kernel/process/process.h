#pragma once

#include <stdint.h>

#include <lib-asym.h>

#include "page_directory/page_directory.h"
#include "vfs/vfs.h"
#include "file_table/file_table.h"

typedef struct
{    
    uint64_t id;

    PageDirectory* pageDirectory;
    FileTable* fileTable;
} Process;

Process* process_new();

void process_free(Process* process);

void* process_allocate_pages(Process* process, void* virtualAddress, uint64_t amount);
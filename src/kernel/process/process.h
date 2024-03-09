#pragma once

#include <stdint.h>

#include <lib-asym.h>

#include "lock/lock.h"
#include "page_directory/page_directory.h"
#include "vfs/vfs.h"
#include "file_table/file_table.h"

typedef struct
{        
    PageDirectory* pageDirectory;
    FileTable* fileTable;
    Lock lock;

    uint64_t id;
    _Atomic uint64_t refCount;
} Process;

Process* process_new();

Process* process_ref(Process* process);

void process_unref(Process* process);

void* process_allocate_pages(Process* process, void* virtualAddress, uint64_t amount);
#include "file_table.h"

#include <libc/string.h>

#include <lib-asym.h>

#include "heap/heap.h"
#include "debug/debug.h"

FileTable* file_table_new(void)
{
    FileTable* fileTable = kmalloc(sizeof(FileTable));
    memset(fileTable, 0, sizeof(FileTable));

    return fileTable;
}

void file_table_free(FileTable* fileTable)
{
    for (uint64_t i = 0; i < FILE_TABLE_LENGTH; i++)
    {
        if (fileTable->files[i] != 0)
        {
            Status status = vfs_close(fileTable->files[i]);
            if (status != STATUS_SUCCESS)
            {
                debug_panic("Failed to close file in table");
            }
        }
    }

    kfree(fileTable);
}

Status file_table_open(FileTable* fileTable, uint64_t* out, const char* path, uint64_t flags)
{    
    File* file;
    Status status = vfs_open(&file, path, flags);
    if (status != STATUS_SUCCESS)
    {            
        return status;
    }
    
    for (uint64_t i = 0; i < FILE_TABLE_LENGTH; i++)
    {
        if (fileTable->files[i] == 0)
        {
            fileTable->files[i] = file;
            (*out) = i;
            return STATUS_SUCCESS;
        }
    }

    return STATUS_FAILURE;
}

File* file_table_get(FileTable* fileTable, uint64_t fd)
{
    if (fd >= FILE_TABLE_LENGTH)
    {
        return 0;
    }
    else if (fileTable->files[fd] != 0)
    {
        return fileTable->files[fd];
    }
    else
    {
        return 0;
    }
}

Status file_table_close(FileTable* fileTable, uint64_t fd)
{
    File* file = file_table_get(fileTable, fd);
    if (file == 0)
    {
        return STATUS_DOES_NOT_EXIST;
    }

    Status status = vfs_close(file);
    if (status != STATUS_SUCCESS)
    {            
        return status;
    }
    
    fileTable->files[fd] = 0;

    return STATUS_SUCCESS;
}
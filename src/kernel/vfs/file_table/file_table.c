#include "file_table.h"

#include <string.h>
#include <errno.h>

#include "heap/heap.h"
#include "sched/sched.h"

static inline void file_cleanup(File* file)
{
    file->disk->cleanup(file);
    kfree(file);
}

void file_table_init(FileTable* table)
{
    memset(table, 0, sizeof(FileTable));
    table->lock = lock_create();
}

void file_table_cleanup(FileTable* table)
{
    for (uint64_t i = 0; i < FILE_TABLE_LENGTH; i++)
    {    
        File* file = table->files[i];
        if (file != NULL && atomic_fetch_sub(&file->ref, 1) <= 1)
        {
            file_cleanup(file);
        }
    }
}

fd_t file_table_open(Disk* disk, uint8_t flags, void* context)
{
    FileTable* table = &sched_process()->fileTable;
    lock_acquire(&table->lock);

    for (uint64_t i = 0; i < FILE_TABLE_LENGTH; i++)
    {
        if (table->files[i] == NULL)
        {
            File* file = kmalloc(sizeof(File));
            file->disk = disk;
            file->context = context;
            file->flags = flags;
            file->position = 0;
            file->ref = 1;
            table->files[i] = file;

            lock_release(&table->lock);
            return i;
        }
    }

    lock_release(&table->lock);
    return ERR;
}


uint64_t file_table_close(fd_t fd)
{
    FileTable* table = &sched_process()->fileTable;
    lock_acquire(&table->lock);

    if (fd >= FILE_TABLE_LENGTH || table->files[fd] == NULL)
    {
        lock_release(&table->lock);
        return ERR;
    }

    File* file = table->files[fd];
    table->files[fd] = NULL;

    if (atomic_fetch_sub(&file->ref, 1) == 1)
    {
        file_cleanup(file);
    }

    lock_release(&table->lock);
    return 0;
}

File* file_table_get(fd_t fd)
{
    FileTable* table = &sched_process()->fileTable;
    lock_acquire(&table->lock);

    if (table->files[fd] == NULL)
    {
        lock_release(&table->lock);
        return NULL;
    }

    File* file = table->files[fd];
    atomic_fetch_add(&file->ref, 1);

    lock_release(&table->lock);
    return file;
}

void file_table_put(File* file)
{
    if (atomic_fetch_sub(&file->ref, 1) <= 1)
    {
        file_cleanup(file);
    }
}
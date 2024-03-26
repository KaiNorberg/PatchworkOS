#include "file_table.h"

#include <string.h>
#include <errno.h>

#include "heap/heap.h"
#include "sched/sched.h"

void file_table_init(FileTable* table)
{
    memset(table, 0, sizeof(FileTable));
    table->lock = lock_create();
}

void file_table_cleanup(FileTable* table)
{
    for (uint64_t i = 0; i < FILE_TABLE_LENGTH; i++)
    {
        /*if (table->files[i] != NULL)
        {
            file_table_deref(table->files[i]);
        }*/
    }
}

/*uint64_t file_table_open(Disk* disk, uint64_t flags, void* context)
{
    FileTable* table = &sched_process()->fileTable;
    lock_acquire(&table->lock);

    for (uint64_t i = 0; i < FILE_TABLE_LENGTH; i++)
    {            
        if (table->files[i] == NULL)
        {
            File* file = kmalloc(sizeof(File));
            file->ref = 1;
            file->disk = disk;
            file->context = context;
            file->position = 0;
            file->flags = flags;

            table->files[i] = file;
            
            lock_release(&table->lock);
            return i;
        }
    }

    lock_release(&table->lock);
    sched_thread()->errno = EMFILE;
    return ERROR;
}

uint64_t file_table_close(uint64_t fd)
{    
    FileTable* table = &sched_process()->fileTable;
    lock_acquire(&table->lock);

    if (table->files[fd] == NULL)
    {
        sched_thread()->errno = EBADF;
        lock_release(&table->lock);
        return NULL;
    }

    File* temp = table->files[fd];
    table->files[fd] = NULL;
    lock_release(&table->lock);

    file_table_deref(temp);
}

File* file_table_get(uint64_t fd)
{
    if (fd >= FILE_TABLE_LENGTH)
    {
        sched_thread()->errno = EBADF;
        return NULL;
    }

    FileTable* table = &sched_process()->fileTable;
    lock_acquire(&table->lock);

    File* file = table->files[fd];
    if (file->ref == 0)
    {
        sched_thread()->errno = EBADF;
        lock_release(&table->lock);
        return NULL;
    }
    file->ref++;

    lock_release(&table->lock);
    return file;
}

void file_table_put(File* file)
{    
    FileTable* table = &sched_process()->fileTable;
    lock_acquire(&table->lock);

    file_table_deref(file);

    lock_release(&table->lock);
}*/
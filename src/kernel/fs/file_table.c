#include <kernel/fs/file_table.h>

#include <kernel/fs/path.h>
#include <kernel/proc/process.h>
#include <kernel/sched/thread.h>

#include <sys/bitmap.h>

void file_table_init(file_table_t* table)
{
    for (uint64_t i = 0; i < CONFIG_MAX_FD; i++)
    {
        table->files[i] = NULL;
    }
    BITMAP_DEFINE_INIT(table->bitmap, CONFIG_MAX_FD);
    lock_init(&table->lock);
}

void file_table_deinit(file_table_t* table)
{
    LOCK_SCOPE(&table->lock);

    for (uint64_t i = 0; i < CONFIG_MAX_FD; i++)
    {
        if (table->files[i] != NULL)
        {
            UNREF(table->files[i]);
            table->files[i] = NULL;
        }
    }
}

file_t* file_table_get(file_table_t* table, fd_t fd)
{
    if (table == NULL)
    {
        return NULL;
    }

    LOCK_SCOPE(&table->lock);

    if (fd >= CONFIG_MAX_FD || table->files[fd] == NULL)
    {
        return NULL;
    }

    return REF(table->files[fd]);
}

fd_t file_table_open(file_table_t* table, file_t* file)
{
    if (table == NULL || file == NULL)
    {
        return FDNONE;
    }

    LOCK_SCOPE(&table->lock);

    uint64_t index = bitmap_find_first_clear(&table->bitmap, 0, CONFIG_MAX_FD);
    if (index >= CONFIG_MAX_FD)
    {
        return FDNONE;
    }

    table->files[index] = REF(file);
    bitmap_set(&table->bitmap, index);
    return (fd_t)index;
}

status_t file_table_close(file_table_t* table, fd_t fd)
{
    if (table == NULL)
    {
        return ERR(VFS, INVAL);
    }

    LOCK_SCOPE(&table->lock);

    if (fd >= CONFIG_MAX_FD || table->files[fd] == NULL)
    {
        return ERR(VFS, BADFD);
    }

    UNREF(table->files[fd]);
    table->files[fd] = NULL;
    bitmap_clear(&table->bitmap, fd);
    return OK;
}

void file_table_close_all(file_table_t* table)
{
    if (table == NULL)
    {
        return;
    }

    LOCK_SCOPE(&table->lock);

    for (uint64_t i = 0; i < CONFIG_MAX_FD; i++)
    {
        if (table->files[i] != NULL)
        {
            UNREF(table->files[i]);
            table->files[i] = NULL;
            bitmap_clear(&table->bitmap, i);
        }
    }
}

void file_table_close_mode(file_table_t* table, mode_t mode)
{
    if (table == NULL)
    {
        return;
    }

    LOCK_SCOPE(&table->lock);

    for (uint64_t i = 0; i < CONFIG_MAX_FD; i++)
    {
        if (table->files[i] != NULL && (table->files[i]->mode & mode))
        {
            UNREF(table->files[i]);
            table->files[i] = NULL;
            bitmap_clear(&table->bitmap, i);
        }
    }
}

void file_table_close_range(file_table_t* table, fd_t min, fd_t max)
{
    if (table == NULL)
    {
        return;
    }

    LOCK_SCOPE(&table->lock);

    for (fd_t fd = min; fd < max && fd < CONFIG_MAX_FD; fd++)
    {
        if (table->files[fd] != NULL)
        {
            UNREF(table->files[fd]);
            table->files[fd] = NULL;
            bitmap_clear(&table->bitmap, fd);
        }
    }
}

bool file_table_set(file_table_t* table, fd_t fd, file_t* file)
{
    if (table == NULL || file == NULL)
    {
        return false;
    }

    LOCK_SCOPE(&table->lock);

    if (fd >= CONFIG_MAX_FD)
    {
        return false;
    }

    if (table->files[fd] != NULL)
    {
        UNREF(table->files[fd]);
        table->files[fd] = NULL;
    }

    table->files[fd] = REF(file);
    bitmap_set(&table->bitmap, fd);
    return true;
}

status_t file_table_dup(file_table_t* table, fd_t oldFd, fd_t* newFd)
{
    if (table == NULL || newFd == NULL)
    {
        return ERR(VFS, INVAL);
    }

    LOCK_SCOPE(&table->lock);

    if (oldFd >= CONFIG_MAX_FD)
    {
        return ERR(VFS, FD_OVERFLOW);
    }

    if (table->files[oldFd] == NULL)
    {
        return ERR(VFS, BADFD);
    }

    if (*newFd == oldFd)
    {
        return OK;
    }

    if (*newFd == FDNONE)
    {
        uint64_t index = bitmap_find_first_clear(&table->bitmap, 0, CONFIG_MAX_FD);
        if (index >= CONFIG_MAX_FD)
        {
            return ERR(VFS, MFILE);
        }
        *newFd = (fd_t)index;
    }
    else if (*newFd >= CONFIG_MAX_FD)
    {
        return ERR(VFS, FD_OVERFLOW);
    }
    else if (table->files[*newFd] != NULL)
    {
        UNREF(table->files[*newFd]);
        table->files[*newFd] = NULL;
    }

    table->files[*newFd] = REF(table->files[oldFd]);
    bitmap_set(&table->bitmap, *newFd);
    return OK;
}

void file_table_copy(file_table_t* dest, file_table_t* src, fd_t min, fd_t max)
{
    if (dest == NULL || src == NULL)
    {
        return;
    }

    LOCK_SCOPE(&src->lock);
    LOCK_SCOPE(&dest->lock);

    for (fd_t i = min; i < max && i < CONFIG_MAX_FD; i++)
    {
        if (src->files[i] == NULL)
        {
            continue;
        }

        if (dest->files[i] != NULL)
        {
            UNREF(dest->files[i]);
            dest->files[i] = NULL;
        }

        dest->files[i] = REF(src->files[i]);
        bitmap_set(&dest->bitmap, i);
    }
}

SYSCALL_DEFINE(SYS_FD_DUP, fd_t oldFd, fd_t newFd)
{
    status_t status = file_table_dup(&process_current()->files, oldFd, &newFd);
    *_result = newFd;
    return status;
}
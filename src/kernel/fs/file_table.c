#include <kernel/fs/file_table.h>

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
        errno = EINVAL;
        return NULL;
    }

    LOCK_SCOPE(&table->lock);

    if (fd >= CONFIG_MAX_FD || table->files[fd] == NULL)
    {
        errno = EBADF;
        return NULL;
    }

    return REF(table->files[fd]);
}

fd_t file_table_alloc(file_table_t* table, file_t* file)
{
    if (table == NULL || file == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    LOCK_SCOPE(&table->lock);

    uint64_t index = bitmap_find_first_clear(&table->bitmap, 0, CONFIG_MAX_FD);
    if (index >= CONFIG_MAX_FD)
    {
        errno = EMFILE;
        return ERR;
    }

    table->files[index] = REF(file);
    bitmap_set(&table->bitmap, index);
    return (fd_t)index;
}

uint64_t file_table_free(file_table_t* table, fd_t fd)
{
    if (table == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    LOCK_SCOPE(&table->lock);

    if (fd >= CONFIG_MAX_FD || table->files[fd] == NULL)
    {
        errno = EBADF;
        return ERR;
    }

    UNREF(table->files[fd]);
    table->files[fd] = NULL;
    bitmap_clear(&table->bitmap, fd);
    return 0;
}

fd_t file_table_set(file_table_t* table, fd_t fd, file_t* file)
{
    if (table == NULL || file == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    LOCK_SCOPE(&table->lock);

    if (fd >= CONFIG_MAX_FD)
    {
        errno = EBADF;
        return ERR;
    }

    if (table->files[fd] != NULL)
    {
        UNREF(table->files[fd]);
        table->files[fd] = NULL;
    }

    table->files[fd] = REF(file);
    bitmap_set(&table->bitmap, fd);
    return fd;
}

fd_t file_table_dup(file_table_t* table, fd_t oldFd)
{
    if (table == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    LOCK_SCOPE(&table->lock);

    if (oldFd >= CONFIG_MAX_FD || table->files[oldFd] == NULL)
    {
        errno = EBADF;
        return ERR;
    }

    uint64_t index = bitmap_find_first_clear(&table->bitmap, 0, CONFIG_MAX_FD);
    if (index >= CONFIG_MAX_FD)
    {
        errno = EMFILE;
        return ERR;
    }

    table->files[index] = REF(table->files[oldFd]);
    bitmap_set(&table->bitmap, index);
    return (fd_t)index;
}

fd_t file_table_dup2(file_table_t* table, fd_t oldFd, fd_t newFd)
{
    if (table == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (oldFd == newFd)
    {
        return newFd;
    }

    LOCK_SCOPE(&table->lock);

    if (oldFd >= CONFIG_MAX_FD || newFd >= CONFIG_MAX_FD || table->files[oldFd] == NULL)
    {
        errno = EBADF;
        return ERR;
    }

    if (table->files[newFd] != NULL)
    {
        UNREF(table->files[newFd]);
        table->files[newFd] = NULL;
    }

    table->files[newFd] = REF(table->files[oldFd]);
    bitmap_set(&table->bitmap, newFd);
    return newFd;
}

SYSCALL_DEFINE(SYS_CLOSE, uint64_t, fd_t fd)
{
    return file_table_free(&sched_process()->fileTable, fd);
}

SYSCALL_DEFINE(SYS_DUP, uint64_t, fd_t oldFd)
{
    return file_table_dup(&sched_process()->fileTable, oldFd);
}

SYSCALL_DEFINE(SYS_DUP2, uint64_t, fd_t oldFd, fd_t newFd)
{
    return file_table_dup2(&sched_process()->fileTable, oldFd, newFd);
}
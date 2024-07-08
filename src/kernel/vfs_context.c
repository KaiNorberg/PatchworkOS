#include "vfs_context.h"

#include <string.h>

#include "sched.h"
#include "vfs.h"

void vfs_context_init(vfs_context_t* context)
{
    memset(context, 0, sizeof(vfs_context_t));
    strcpy(context->cwd, "sys:");
    lock_init(&context->lock);
}

void vfs_context_cleanup(vfs_context_t* context)
{
    for (uint64_t i = 0; i < CONFIG_MAX_FILE; i++)
    {
        file_t* file = context->files[i];
        if (file != NULL)
        {
            file_deref(file);
        }
    }
}

fd_t vfs_context_open(file_t* file)
{
    vfs_context_t* context = &sched_process()->vfsContext;
    LOCK_GUARD(&context->lock);

    for (fd_t fd = 0; fd < CONFIG_MAX_FILE; fd++)
    {
        if (context->files[fd] == NULL)
        {
            context->files[fd] = file;
            return fd;
        }
    }

    return ERROR(EMFILE);
}

uint64_t vfs_context_close(fd_t fd)
{
    vfs_context_t* context = &sched_process()->vfsContext;
    LOCK_GUARD(&context->lock);

    if (fd >= CONFIG_MAX_FILE || context->files[fd] == NULL)
    {
        return ERROR(EBADF);
    }

    file_t* file = context->files[fd];
    context->files[fd] = NULL;
    file_deref(file);
    return 0;
}

file_t* vfs_context_get(fd_t fd)
{
    vfs_context_t* context = &sched_process()->vfsContext;
    LOCK_GUARD(&context->lock);

    if (context->files[fd] == NULL)
    {
        return NULLPTR(EBADF);
    }

    return file_ref(context->files[fd]);
}

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

void vfs_context_deinit(vfs_context_t* context)
{
    for (uint64_t i = 0; i < CONFIG_MAX_FD; i++)
    {
        if (context->files[i] != NULL)
        {
            file_deref(context->files[i]);
        }
    }
}

fd_t vfs_context_open(vfs_context_t* context, file_t* file)
{
    LOCK_DEFER(&context->lock);

    for (fd_t fd = 0; fd < CONFIG_MAX_FD; fd++)
    {
        if (context->files[fd] == NULL)
        {
            context->files[fd] = file_ref(file);
            return fd;
        }
    }

    return ERROR(EMFILE);
}

uint64_t vfs_context_close(vfs_context_t* context, fd_t fd)
{
    LOCK_DEFER(&context->lock);

    if (fd >= CONFIG_MAX_FD || context->files[fd] == NULL)
    {
        return ERROR(EBADF);
    }

    file_t* file = context->files[fd];
    context->files[fd] = NULL;
    file_deref(file);
    return 0;
}

fd_t vfs_context_openat(vfs_context_t* context, fd_t fd, file_t* file)
{
    LOCK_DEFER(&context->lock);

    if (fd >= CONFIG_MAX_FD)
    {
        return ERROR(EINVAL);
    }

    if (context->files[fd] != NULL)
    {
        return ERROR(EMFILE);
    }

    context->files[fd] = file_ref(file);
    return fd;
}

file_t* vfs_context_get(vfs_context_t* context, fd_t fd)
{
    LOCK_DEFER(&context->lock);

    if (context->files[fd] == NULL)
    {
        return ERRPTR(EBADF);
    }

    return file_ref(context->files[fd]);
}

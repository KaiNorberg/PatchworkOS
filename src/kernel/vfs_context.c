#include "vfs_context.h"

#include <string.h>

#include "sched.h"

void vfs_context_init(VfsContext* context)
{
    memset(context, 0, sizeof(VfsContext));
    strcpy(context->cwd, "A:/");
    lock_init(&context->lock);
}

void vfs_context_cleanup(VfsContext* context)
{
    for (uint64_t i = 0; i < CONFIG_MAX_FILE; i++)
    {    
        File* file = context->files[i];
        if (file != NULL)
        {
            file_deref(file);
        }
    }
}

uint64_t vfs_context_open(File* file)
{
    VfsContext* context = &sched_process()->vfsContext;
    LOCK_GUARD(&context->lock);

    for (uint64_t fd = 0; fd < CONFIG_MAX_FILE; fd++)
    {
        if (context->files[fd] == NULL)
        {
            context->files[fd] = file;
            return fd;
        }
    }

    return ERROR(EMFILE);
}

uint64_t vfs_context_close(uint64_t fd)
{
    VfsContext* context = &sched_process()->vfsContext;
    LOCK_GUARD(&context->lock);

    if (fd >= CONFIG_MAX_FILE || context->files[fd] == NULL)
    {
        return ERROR(EBADF);
    }

    File* file = context->files[fd];
    context->files[fd] = NULL;
    file_deref(file);
    return 0;
}

File* vfs_context_get(uint64_t fd)
{
    VfsContext* context = &sched_process()->vfsContext;
    LOCK_GUARD(&context->lock);

    if (context->files[fd] == NULL)
    {
        return NULLPTR(EBADF);
    }

    return file_ref(context->files[fd]);
}
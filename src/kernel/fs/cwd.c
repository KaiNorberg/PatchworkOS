#include <kernel/fs/cwd.h>

#include <kernel/fs/namespace.h>
#include <kernel/sched/thread.h>

void cwd_init(cwd_t* cwd)
{
    cwd->path = PATH_EMPTY;
    lock_init(&cwd->lock);
}

void cwd_deinit(cwd_t* cwd)
{
    lock_acquire(&cwd->lock);
    path_put(&cwd->path);
    lock_release(&cwd->lock);
}

path_t cwd_get(cwd_t* cwd, namespace_handle_t* ns)
{
    path_t result = PATH_EMPTY;

    lock_acquire(&cwd->lock);

    if (cwd->path.dentry == NULL || cwd->path.mount == NULL)
    {
        assert(cwd->path.dentry == NULL && cwd->path.mount == NULL);
        namespace_get_root(ns, &result);
        lock_release(&cwd->lock);
        return result;
    }

    path_copy(&result, &cwd->path);
    lock_release(&cwd->lock);

    return result;
}

void cwd_set(cwd_t* cwd, const path_t* newPath)
{
    lock_acquire(&cwd->lock);
    path_copy(&cwd->path, newPath);
    lock_release(&cwd->lock);
}

void cwd_clear(cwd_t* cwd)
{
    lock_acquire(&cwd->lock);
    path_put(&cwd->path);
    lock_release(&cwd->lock);
}

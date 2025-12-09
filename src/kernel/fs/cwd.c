#include <kernel/fs/cwd.h>

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

path_t cwd_get(cwd_t* cwd)
{
    path_t result = PATH_EMPTY;

    lock_acquire(&cwd->lock);

    if (cwd->path.dentry == NULL || cwd->path.mount == NULL)
    {
        assert(cwd->path.dentry == NULL && cwd->path.mount == NULL);
        namespace_t* kernelNs = &process_get_kernel()->ns;

        if (namespace_get_root_path(kernelNs, &result) == ERR)
        {
            lock_release(&cwd->lock);
            return PATH_EMPTY;
        }
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

SYSCALL_DEFINE(SYS_CHDIR, uint64_t, const char* pathString)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    pathname_t pathname;
    if (thread_copy_from_user_pathname(thread, &pathname, pathString) == ERR)
    {
        return ERR;
    }

    path_t cwd = cwd_get(&process->cwd);
    PATH_DEFER(&cwd);

    path_t path = PATH_EMPTY;
    if (path_walk(&path, &pathname, &cwd, &process->ns) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&path);

    inode_t* inode = dentry_inode_get(path.dentry);
    if (inode == NULL)
    {
        errno = ENOENT;
        return ERR;
    }
    DEREF_DEFER(inode);

    if (inode->type != INODE_DIR)
    {
        errno = ENOTDIR;
        return ERR;
    }

    cwd_set(&process->cwd, &path);
    return 0;
}

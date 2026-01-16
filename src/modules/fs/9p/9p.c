#include <kernel/fs/filesystem.h>
#include <kernel/module/module.h>

#include <errno.h>
#include <kernel/sched/sched.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 9P Filesystems.
 * @defgroup modules_fs_9p 9P Filesystem
 * @ingroup modules_fs
 *
 * This module provides an implementation of the 9P filesystem protocol where the kernel acts as a client to a 9P
 * server, allowing the 9P server to be mounted as a filesystem within the kernel's VFS.
 *
 * The 9p filesystem supports the following options:
 * - `in`: The file descriptor to read 9P messages from.
 * - `out`: The file descriptor to write 9P messages to.
 * - `version`: The 9P protocol version to use, currently only `9P2000` is supported, the default is `9P2000`.
 *
 * @see libstd_sys_9p for the 9P protocol definitions.
 * @see http://rfc.nop.hu/plan9/rfc9p.pdf for the 9P protocol specification.
 *
 * @{
 */

typedef struct
{
    file_t* in;
    file_t* out;
} ninep_t;

static void ninep_super_cleanup(superblock_t* sb)
{
    ninep_t* ninep = sb->data;
    if (ninep == NULL)
    {
        return;
    }

    UNREF(ninep->in);
    UNREF(ninep->out);
    free(ninep);
    sb->data = NULL;
}

static superblock_ops_t superOps = {
    .cleanup = ninep_super_cleanup,
};

static dentry_t* ninep_mount(filesystem_t* fs, const char* options, void* data)
{
    UNUSED(data);

    if (options == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    fd_t in = ERR;
    fd_t out = ERR;
    const char* version = "9P2000";

    char* key;
    char* value;
    OPTIONS_FOR_EACH(options, key, value)
    {
        if (strcmp(key, "in") == 0)
        {
            in = atoi(value);
        }
        else if (strcmp(key, "out") == 0)
        {
            out = atoi(value);
        }
        else if (strcmp(key, "version") == 0)
        {
            version = value;
        }
        else
        {
            errno = EINVAL;
            return NULL;
        }
    }

    if (in == ERR || out == ERR)
    {
        errno = EINVAL;
        return NULL;
    }

    if (strcmp(version, "9P2000") != 0)
    {
        errno = ENOSYS;
        return NULL;
    }

    superblock_t* superblock = superblock_new(fs, &superOps, NULL);
    if (superblock == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(superblock);

    ninep_t* ninep = malloc(sizeof(ninep_t));
    if (ninep == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    process_t* process = process_current();
    assert(process != NULL);

    ninep->in = file_table_get(&process->fileTable, in);
    if (ninep->in == NULL)
    {
        free(ninep);
        return NULL;
    }
    ninep->out = file_table_get(&process->fileTable, out);
    if (ninep->out == NULL)
    {
        UNREF(ninep->in);
        return NULL;
    }

    superblock->data = ninep;

    inode_t* inode = inode_new(superblock, 0, INODE_DIR, NULL, NULL);
    if (inode == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(inode);

    dentry_t* dentry = dentry_new(superblock, NULL, NULL);
    if (dentry == NULL)
    {
        return NULL;
    }

    dentry_make_positive(dentry, inode);

    superblock->root = dentry;
    return superblock->root;
}

static filesystem_t ninep = {
    .name = "9p",
    .mount = ninep_mount,
};

/** @} */

uint64_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_LOAD:
        if (filesystem_register(&ninep) == ERR)
        {
            return ERR;
        }
        break;
    case MODULE_EVENT_UNLOAD:
        filesystem_unregister(&ninep);
        break;
    default:
        break;
    }
    return 0;
}

MODULE_INFO("9P Filesystem", "Kai Norberg", "A implementation of the 9P filesystem", OS_VERSION, "MIT", "BOOT_ALWAYS");
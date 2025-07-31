#include "shmem.h"

#include "fs/ctl.h"
#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "proc/process.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/list.h>

static atomic_uint64_t newId = ATOMIC_VAR_INIT(0);

static sysfs_dir_t shmemDir;
static sysfs_file_t newFile;

static shmem_object_t* shmem_object_new(void);

static void shmem_vmm_callback(void* private)
{
    shmem_object_t* shmem = private;
    if (shmem == NULL)
    {
        return;
    }

    DEREF(shmem);
}

static void shmem_object_free(shmem_object_t* shmem)
{
    if (shmem == NULL)
    {
        return;
    }

    sysfs_file_deinit(&shmem->file);
}

static bool shmem_object_is_access_allowed(shmem_object_t* shmem, process_t* proccess)
{
    if (proccess->id == shmem->owner || process_is_child(proccess, shmem->owner))
    {
        return true;
    }

    shmem_allowed_process_t* allowed;
    LIST_FOR_EACH(allowed, &shmem->allowedProcesses, entry)
    {
        if (allowed->pid == proccess->id)
        {
            return true;
        }
    }

    return false;
}

static void* shmem_object_allocate_pages(shmem_object_t* shmem, uint64_t pageAmount, space_t* space, void* address,
    prot_t prot)
{
    shmem->pages = heap_alloc(sizeof(void*) * pageAmount, HEAP_VMM);
    if (shmem->pages == NULL)
    {
        return NULL;
    }
    shmem->pageAmount = pageAmount;

    for (uint64_t i = 0; i < pageAmount; i++)
    {
        shmem->pages[i] = pmm_alloc();
        if (shmem->pages[i] == NULL)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                pmm_free(shmem->pages[j]);
            }

            heap_free(shmem->pages);
            shmem->pages = NULL;
            shmem->pageAmount = 0;
            return NULL;
        }
    }

    void* virtAddr =
        vmm_map_pages(space, address, shmem->pages, shmem->pageAmount, prot, shmem_vmm_callback, REF(shmem));
    if (virtAddr == NULL)
    {
        for (uint64_t i = 0; i < shmem->pageAmount; i++)
        {
            pmm_free(shmem->pages[i]);
        }

        heap_free(shmem->pages);
        shmem->pages = NULL;
        shmem->pageAmount = 0;
        return NULL;
    }

    return virtAddr;
}

static void* shmem_mmap(file_t* file, void* address, uint64_t length, prot_t prot)
{
    shmem_object_t* shmem = file->private;
    if (shmem == NULL)
    {
        errno = EINVAL;
        return NULL;
    }
    LOCK_SCOPE(&shmem->lock);

    process_t* process = sched_process();

    if (!shmem_object_is_access_allowed(shmem, process))
    {
        errno = EACCES;
        return NULL;
    }

    space_t* space = &process->space;

    uint64_t pageAmount = BYTES_TO_PAGES(length);
    if (pageAmount == 0)
    {
        errno = EINVAL;
        return NULL;
    }

    if (shmem->pageAmount == 0) // First call to mmap()
    {
        assert(shmem->pages == NULL);
        return shmem_object_allocate_pages(shmem, pageAmount, space, address, prot);
    }
    else
    {
        assert(shmem->pages != NULL);
        return vmm_map_pages(space, address, shmem->pages, MIN(pageAmount, shmem->pageAmount), prot, shmem_vmm_callback,
            REF(shmem));
    }
}

static uint64_t shmem_ctl_grant(file_t* file, uint64_t argc, const char** argv)
{
    shmem_object_t* shmem = file->private;
    if (shmem == NULL)
    {
        errno = EINVAL;
        return ERR;
    }
    LOCK_SCOPE(&shmem->lock);

    process_t* process = sched_process();

    if (!shmem_object_is_access_allowed(shmem, process))
    {
        errno = EACCES;
        return ERR;
    }

    pid_t pid = strtoll(argv[1], NULL, 10);
    if (errno == ERANGE)
    {
        return ERR;
    }

    shmem_allowed_process_t* allowed = heap_alloc(sizeof(shmem_allowed_process_t), HEAP_NONE);
    if (allowed == NULL)
    {
        return ERR;
    }

    list_entry_init(&allowed->entry);
    allowed->pid = pid;

    list_push(&shmem->allowedProcesses, &allowed->entry);
    return 0;
}

static uint64_t shmem_ctl_revoke(file_t* file, uint64_t argc, const char** argv)
{
    shmem_object_t* shmem = file->private;
    if (shmem == NULL)
    {
        errno = EINVAL;
        return ERR;
    }
    LOCK_SCOPE(&shmem->lock);

    process_t* process = sched_process();

    if (!shmem_object_is_access_allowed(shmem, process))
    {
        errno = EACCES;
        return ERR;
    }

    pid_t pid = strtoll(argv[1], NULL, 10);
    if (errno == ERANGE)
    {
        return ERR;
    }

    shmem_allowed_process_t* allowed;
    LIST_FOR_EACH(allowed, &shmem->allowedProcesses, entry)
    {
        if (allowed->pid == pid)
        {
            list_remove(&shmem->allowedProcesses, &allowed->entry);
            return 0;
        }
    }

    errno = ENOENT;
    return ERR;
}

CTL_STANDARD_WRITE_DEFINE(shmem_write,
    (ctl_array_t){
        {"grant", shmem_ctl_grant, 2, 2},
        {"revoke", shmem_ctl_revoke, 2, 2},
        {0},
    });

static uint64_t shmem_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    shmem_object_t* shmem = file->private;
    if (shmem == NULL)
    {
        errno = EINVAL;
        return ERR;
    }
    LOCK_SCOPE(&shmem->lock);

    process_t* process = sched_process();

    if (!shmem_object_is_access_allowed(shmem, process))
    {
        errno = EACCES;
        return ERR;
    }

    uint64_t size = strlen(shmem->id) + 1;
    return BUFFER_READ(buffer, count, offset, shmem->id, size);
}

static uint64_t shmem_open(file_t* file)
{
    shmem_object_t* shmem = file->inode->private;
    if (shmem == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    file->private = REF(shmem);
    return 0;
}

static void shmem_close(file_t* file)
{
    shmem_object_t* shmem = file->private;
    if (shmem == NULL)
    {
        return;
    }

    DEREF(shmem);
}

static file_ops_t normalFileOps = {
    .open = shmem_open,
    .read = shmem_read,
    .write = shmem_write,
    .mmap = shmem_mmap,
    .close = shmem_close,
};

static void shmem_inode_cleanup(inode_t* inode)
{
    shmem_object_t* shmem = inode->private;
    if (shmem == NULL)
    {
        return;
    }

    if (shmem->pageAmount > 0)
    {
        assert(shmem->pages != NULL);
        for (uint64_t i = 0; i < shmem->pageAmount; i++)
        {
            pmm_free(shmem->pages[i]);
        }
        shmem->pageAmount = 0;
        shmem->pages = NULL;
    }
    heap_free(shmem);
    inode->private = NULL;
}

static inode_ops_t inodeOps = {
    .cleanup = shmem_inode_cleanup,
};

static shmem_object_t* shmem_object_new(void)
{
    shmem_object_t* shmem = heap_alloc(sizeof(shmem_object_t), HEAP_NONE);
    if (shmem == NULL)
    {
        return NULL;
    }

    ref_init(&shmem->ref, shmem_object_free);
    snprintf(shmem->id, sizeof(shmem->id), "%lu", atomic_fetch_add(&newId, 1));
    list_init(&shmem->allowedProcesses);
    shmem->pageAmount = 0;
    shmem->pages = NULL;
    lock_init(&shmem->lock);
    shmem->owner = sched_process()->id; // Set owner to the current process

    if (sysfs_file_init(&shmem->file, &shmemDir, shmem->id, &inodeOps, &normalFileOps, shmem) == ERR)
    {
        DEREF(shmem);
        return NULL;
    }

    return shmem;
}

static uint64_t shmem_new_open(file_t* file)
{
    shmem_object_t* shmem = shmem_object_new();
    if (shmem == NULL)
    {
        return ERR;
    }

    file->ops = &normalFileOps;
    file->private = shmem;
    return 0;
}

static file_ops_t newFileOps = {
    .open = shmem_new_open,
    .close = shmem_close,
};

void shmem_init(void)
{
    if (sysfs_dir_init(&shmemDir, sysfs_get_default(), "shmem", NULL, NULL) == ERR)
    {
        panic(NULL, "Failed to initialize shmem directory");
    }
    if (sysfs_file_init(&newFile, &shmemDir, "new", NULL, &newFileOps, NULL) == ERR)
    {
        panic(NULL, "Failed to initialize shmem file");
    }
}

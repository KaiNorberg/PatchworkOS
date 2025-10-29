#include <kernel/ipc/shmem.h>

#include <kernel/fs/ctl.h>
#include <kernel/fs/sysfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/panic.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>
#include <kernel/proc/process.h>

#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>

static dentry_t* shmemDir = NULL;
static dentry_t* newFile = NULL;

static void shmem_object_free(shmem_object_t* shmem)
{
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
    free(shmem);
}

static shmem_object_t* shmem_object_new(void)
{
    shmem_object_t* shmem = malloc(sizeof(shmem_object_t));
    if (shmem == NULL)
    {
        return NULL;
    }
    ref_init(&shmem->ref, shmem_object_free);
    shmem->pageAmount = 0;
    shmem->pages = NULL;
    lock_init(&shmem->lock);

    return shmem;
}

static void shmem_vmm_callback(void* private)
{
    shmem_object_t* shmem = private;
    if (shmem == NULL)
    {
        return;
    }

    DEREF(shmem);
}

static void* shmem_object_allocate_pages(shmem_object_t* shmem, uint64_t pageAmount, space_t* space, void* address,
    pml_flags_t flags)
{
    shmem->pages = malloc(sizeof(void*) * pageAmount);
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

            free(shmem->pages);
            shmem->pages = NULL;
            shmem->pageAmount = 0;
            return NULL;
        }
    }

    void* virtAddr =
        vmm_map_pages(space, address, shmem->pages, shmem->pageAmount, flags, shmem_vmm_callback, REF(shmem));
    if (virtAddr == NULL)
    {
        for (uint64_t i = 0; i < shmem->pageAmount; i++)
        {
            pmm_free(shmem->pages[i]);
        }

        free(shmem->pages);
        shmem->pages = NULL;
        shmem->pageAmount = 0;
        return NULL;
    }

    return virtAddr;
}

static uint64_t shmem_open(file_t* file)
{
    shmem_object_t* shmem = shmem_object_new();
    if (shmem == NULL)
    {
        return ERR;
    }

    file->private = shmem;
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

static void* shmem_mmap(file_t* file, void* address, uint64_t length, uint64_t* offset, pml_flags_t flags)
{
    shmem_object_t* shmem = file->private;
    if (shmem == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    LOCK_SCOPE(&shmem->lock);

    process_t* process = sched_process_unsafe();
    space_t* space = &process->space;

    uint64_t pageAmount = BYTES_TO_PAGES(length);
    if (pageAmount == 0)
    {
        errno = EINVAL;
        return NULL;
    }

    if (shmem->pageAmount == 0) // First call to mmap()
    {
        if (*offset != 0)
        {
            errno = EINVAL;
            return NULL;
        }

        assert(shmem->pages == NULL);
        return shmem_object_allocate_pages(shmem, pageAmount, space, address, flags);
    }
    else
    {
        assert(shmem->pages != NULL);

        if (*offset >= shmem->pageAmount * PAGE_SIZE)
        {
            errno = EINVAL;
            return NULL;
        }

        if (*offset % PAGE_SIZE != 0)
        {
            errno = EINVAL;
            return NULL;
        }

        uint64_t pageOffset = *offset / PAGE_SIZE;
        uint64_t availablePages = shmem->pageAmount - pageOffset;
        return vmm_map_pages(space, address, &shmem->pages[pageOffset], MIN(pageAmount, availablePages), flags,
            shmem_vmm_callback, REF(shmem));
    }
}

static file_ops_t fileOps = {
    .open = shmem_open,
    .close = shmem_close,
    .mmap = shmem_mmap,
};

void shmem_init(void)
{
    shmemDir = sysfs_dir_new(NULL, "shmem", NULL, NULL);
    if (shmemDir == NULL)
    {
        panic(NULL, "Failed to create /dev/shmem directory");
    }

    newFile = sysfs_file_new(shmemDir, "new", NULL, &fileOps, NULL);
    if (newFile == NULL)
    {
        DEREF(shmemDir);
        panic(NULL, "Failed to create /dev/shmem/new file");
    }
}

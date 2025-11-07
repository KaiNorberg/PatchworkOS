#include <kernel/fs/ctl.h>
#include <kernel/fs/sysfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/panic.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>
#include <kernel/log/log.h>
#include <kernel/proc/process.h>
#include <kernel/module/module.h>
#include <kernel/sched/sched.h>
#include <kernel/sync/lock.h>
#include <kernel/utils/ref.h>

#include <errno.h>
#include <stdlib.h>

#include <sys/list.h>
/**
 * @brief Shared Memory
 * @ingroup kernel_ipc
 * @defgroup kernel_ipc_shmem Shared Memory
 *
 * Shared memory is exposed in the `/dev/shmem` directory. Shared memory allows multiple processes to share a section of
 * memory for inter-process communication (IPC).
 *
 * ## Creating Shared Memory
 *
 * Shared memory objects are created using the `/dev/shmem/new` file. Opening this file using `open()` will create a new
 * anonymous shared memory object and return a file descriptor to it.
 *
 * ## Using Shared Memory
 *
 * Shared memory objects can be mapped to the current process's address space using the `mmap()` system call. The first
 * call to `mmap()` will decide the size of the shared memory object. Subsequent calls to `mmap()` will map the existing
 * shared memory object.
 *
 * @{
 */

/**
 * @brief Represents a shared memory object.
 */
typedef struct
{
    ref_t ref;
    uint64_t pageAmount;
    void** pages;
    lock_t lock;
} shmem_object_t;

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

static uint64_t shmem_init(void)
{
    shmemDir = sysfs_dir_new(NULL, "shmem", NULL, NULL);
    if (shmemDir == NULL)
    {
        LOG_ERR("failed to create /dev/shmem directory");
        return ERR;
    }

    newFile = sysfs_file_new(shmemDir, "new", NULL, &fileOps, NULL);
    if (newFile == NULL)
    {
        DEREF(shmemDir);
        LOG_ERR("failed to create /dev/shmem/new file");
        return ERR; 
    }

    return 0;
}

static void shmem_deinit(void)
{
    DEREF(newFile);
    DEREF(shmemDir);
}

/** @} */

uint64_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_LOAD:
        if (shmem_init() == ERR)
        {
            return ERR;
        }
        break;
    case MODULE_EVENT_UNLOAD:
        shmem_deinit();
        break;
    default:
        break;
    }

    return 0;
}

MODULE_INFO("Shared Memory Driver", "Kai Norberg", "A shared memory device driver", OS_VERSION, "MIT", "LOAD_ON_BOOT");
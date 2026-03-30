#include <kernel/fs/ctl.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/vnode.h>
#include <kernel/io/irp.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>
#include <kernel/module/module.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>
#include <kernel/sync/lock.h>
#include <kernel/utils/ref.h>

#include <errno.h>
#include <stdlib.h>

#include <libc/list.h>

/**
 * @brief Shared Memory
 * @defgroup kernel_ipc_shmem Shared Memory
 * @ingroup kernel_ipc
 *
 * Shared memory is exposed in the `/dev/shmem` directory. Shared memory allows multiple processes to share a section of
 * memory for inter-process communication (IPC).
 *
 * ## Creating Shared Memory
 *
 * Shared memory objects are created using the `/dev/shmem/clone` file. Opening this file will create a new
 * anonymous shared memory object and return a file descriptor to it.
 *
 * ## Using Shared Memory
 *
 * Shared memory objects can be mapped to the current process's address space. The first mapping operation will decide
 * the size of the shared memory object. Subsequent mapping will map the existing shared memory object.
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
    pfn_t* pages;
    lock_t lock;
} shmem_object_t;

static dentry_t* dir = NULL;
static dentry_t* clone = NULL;

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

static void shmem_vmm_callback(void* data)
{
    shmem_object_t* shmem = data;
    if (shmem == NULL)
    {
        return;
    }

    UNREF(shmem);
}

static status_t shmem_object_allocate_pages(shmem_object_t* shmem, uint64_t pageAmount, space_t* space, void** address,
    pml_flags_t flags)
{
    shmem->pages = malloc(sizeof(pfn_t) * pageAmount);
    if (shmem->pages == NULL)
    {
        return ERR(DRIVER, NOMEM);
    }
    shmem->pageAmount = pageAmount;

    for (uint64_t i = 0; i < pageAmount; i++)
    {
        shmem->pages[i] = pmm_alloc();
        if (shmem->pages[i] == PFN_INVALID)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                pmm_free(shmem->pages[j]);
            }

            free(shmem->pages);
            shmem->pages = NULL;
            shmem->pageAmount = 0;
            return ERR(DRIVER, NOMEM);
        }
    }

    status_t status =
        vmm_map_pages(space, address, shmem->pages, shmem->pageAmount, flags, shmem_vmm_callback, REF(shmem));
    if (IS_ERR(status))
    {
        UNREF(shmem);
        for (uint64_t i = 0; i < shmem->pageAmount; i++)
        {
            pmm_free(shmem->pages[i]);
        }

        free(shmem->pages);
        shmem->pages = NULL;
        shmem->pageAmount = 0;
        return status;
    }

    return OK;
}

static status_t shmem_open(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;

    shmem_object_t* shmem = shmem_object_new();
    if (shmem == NULL)
    {
        return ERR(DRIVER, NOMEM);
    }

    file->data = shmem;
    return OK;
}

static status_t shmem_close(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;
    shmem_object_t* shmem = file->data;
    if (shmem == NULL)
    {
        return OK;
    }

    UNREF(shmem);
    return OK;
}

static status_t shmem_mmap(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);

    file_t* file = frame->file;
    void* address = frame->mmap.address;
    size_t length = frame->mmap.length;
    pml_flags_t flags = frame->mmap.flags;
    size_t offset = frame->mmap.offset;

    shmem_object_t* shmem = file->data;
    if (shmem == NULL)
    {
        return ERR(DRIVER, INVAL);
    }

    LOCK_SCOPE(&shmem->lock);

    process_t* process = irp->process;
    space_t* space = &process->space;

    uint64_t pageAmount = BYTES_TO_PAGES(length);
    if (pageAmount == 0)
    {
        return ERR(DRIVER, INVAL);
    }

    if (shmem->pageAmount == 0) // First call to mmap()
    {
        if (offset != 0)
        {
            return ERR(DRIVER, INVAL);
        }

        assert(shmem->pages == NULL);
        status_t status = shmem_object_allocate_pages(shmem, pageAmount, space, &address, flags);
        if (IS_ERR(status))
        {
            return status;
        }

        irp->result = (uint64_t)address;
        return OK;
    }

    assert(shmem->pages != NULL);

    if (offset >= shmem->pageAmount * PAGE_SIZE)
    {
        return ERR(DRIVER, INVAL);
    }

    if (offset % PAGE_SIZE != 0)
    {
        return ERR(DRIVER, INVAL);
    }

    uint64_t pageOffset = offset / PAGE_SIZE;
    uint64_t availablePages = shmem->pageAmount - pageOffset;
    status_t status = vmm_map_pages(space, &address, &shmem->pages[pageOffset], MIN(pageAmount, availablePages), flags,
        shmem_vmm_callback, REF(shmem));
    if (IS_ERR(status))
    {
        UNREF(shmem);
        return status;
    }

    irp->result = (uint64_t)address;
    return OK;
}

static vnode_class_t fileClass = {
    .name = "shmem file",
    .type = FILE_TYPE_REGULAR,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_OPEN] = shmem_open,
            [IRP_MJ_CLOSE] = shmem_close,
            [IRP_MJ_MMAP] = shmem_mmap,
        },
};

static vnode_class_t dirClass = {
    .name = "shmem dir",
    .type = FILE_TYPE_DIRECTORY,
    .handlers =
        {
            VNODE_DIR_HANDLERS(),
        },
};

static status_t shmem_init(void)
{
    dir = devfs_dentry_new(NULL, "shmem", &dirClass, NULL);
    if (dir == NULL)
    {
        LOG_ERR("failed to create /dev/shmem directory");
        return ERR(DRIVER, IO);
    }

    clone = devfs_dentry_new(dir, "clone", &fileClass, NULL);
    if (clone == NULL)
    {
        UNREF(dir);
        LOG_ERR("failed to create /dev/shmem/clone file");
        return ERR(DRIVER, IO);
    }

    return OK;
}

static void shmem_deinit(void)
{
    UNREF(clone);
    clone = NULL;
    UNREF(dir);
    dir = NULL;
}

/** @} */

status_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_LOAD:
        return shmem_init();
    case MODULE_EVENT_UNLOAD:
        shmem_deinit();
        break;
    default:
        break;
    }

    return OK;
}
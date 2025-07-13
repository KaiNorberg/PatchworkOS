#include "shmem.h"

#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "sched/thread.h"

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

static atomic_uint64_t newId = ATOMIC_VAR_INIT(0);

static sysfs_dir_t shmemDir;
static sysfs_file_t newFile;


static void shmem_free(shmem_t* shmem)
{
    sysfs_file_deinit(&shmem->obj);
}

static uint64_t shmem_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    shmem_t* shmem = file->private;
    uint64_t size = strlen(shmem->id) + 1;
    return BUFFER_READ(buffer, count, offset, shmem->id, size);
}

static void shmem_vmm_callback(void* private)
{
    shmem_t* shmem = private;
    DEREF(shmem);
}

static void* shmem_mmap(file_t* file, void* address, uint64_t length, prot_t prot)
{
    shmem_t* shmem = file->private;
    LOCK_SCOPE(&shmem->lock);

    process_t* process = sched_process();
    space_t* space = &process->space;

    uint64_t pageAmount = BYTES_TO_PAGES(length);
    if (pageAmount == 0)
    {
        errno = EINVAL;
        return NULL;
    }

    if (shmem->segment == NULL) // First call to mmap()
    {
        shmem_segment_t* segment = heap_alloc(sizeof(shmem_segment_t) + sizeof(void*) * pageAmount, HEAP_NONE);
        if (segment == NULL)
        {
            return NULL;
        }

        segment->pageAmount = pageAmount;
        for (uint64_t i = 0; i < pageAmount; i++)
        {
            segment->pages[i] = pmm_alloc();
            if (segment->pages[i] == NULL)
            {
                for (uint64_t j = 0; j < i; j++)
                {
                    pmm_free(segment->pages[i]);
                }
                heap_free(segment);
                return NULL;
            }
        }

        void* result = vmm_map_pages(space, address, segment->pages, segment->pageAmount, prot, shmem_vmm_callback,
            REF(shmem));
        if (result == NULL)
        {
            for (uint64_t i = 0; i < segment->pageAmount; i++)
            {
                pmm_free(segment->pages[i]);
            }
            heap_free(segment);
            return NULL;
        }

        shmem->segment = segment;
        return result;
    }
    else
    {
        shmem_segment_t* segment = shmem->segment;

        return vmm_map_pages(space, address, segment->pages, segment->pageAmount, prot, shmem_vmm_callback,
            REF(shmem));
    }
}

static uint64_t shmem_open(file_t* file)
{
    shmem_t* shmem = file->inode->private;
    file->private = REF(shmem);
    return 0;
}

static void shmem_file_cleanup(file_t* file)
{
    shmem_t* shmem = file->private;
    if (shmem != NULL)
    {
        DEREF(shmem);
    }
}

static file_ops_t normalFileOps = {
    .open = shmem_open,
    .read = shmem_read,
    .mmap = shmem_mmap,
    .cleanup = shmem_file_cleanup,
};

static void shmem_inode_cleanup(inode_t* inode)
{
    shmem_t* shmem = inode->private;
    if (shmem == NULL)
    {
        return;
    }

    if (shmem->segment != NULL)
    {
        for (uint64_t i = 0; i < shmem->segment->pageAmount; i++)
        {
            pmm_free(shmem->segment->pages[i]);
        }
        heap_free(shmem->segment);
    }
    heap_free(shmem);
}

static inode_ops_t inodeOps = {
    .cleanup = shmem_inode_cleanup,
};

static uint64_t shmem_new_open(file_t* file)
{
    shmem_t* shmem = heap_alloc(sizeof(shmem_t), HEAP_NONE);
    if (shmem == NULL)
    {
        return ERR;
    }

    ref_init(&shmem->ref, shmem_free);
    lock_init(&shmem->lock);
    ulltoa(atomic_fetch_add(&newId, 1), shmem->id, 10);
    shmem->segment = NULL;
    if (sysfs_file_init(&shmem->obj, &shmemDir, shmem->id, &inodeOps, &normalFileOps, shmem) == ERR)
    {
        heap_free(shmem);
        return ERR;
    }

    file->ops = &normalFileOps;
    file->private = shmem;
    return 0;
}

static file_ops_t newFileOps = {
    .open = shmem_new_open,
    .cleanup = shmem_file_cleanup,
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

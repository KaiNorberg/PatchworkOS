#include "shmem.h"

#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "log/log.h"
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

static sysdir_t dir;
static sysobj_t new;

static shmem_t* shmem_ref(shmem_t* shmem)
{
    atomic_fetch_add(&shmem->ref, 1);
    return shmem;
}

static void shmem_on_free(sysobj_t* sysobj)
{
    shmem_t* shmem = sysobj->private;
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

static void shmem_deref(shmem_t* shmem)
{
    if (atomic_fetch_sub(&shmem->ref, 1) <= 1)
    {
        sysobj_deinit(&shmem->obj, shmem_on_free);
    }
}

static uint64_t shmem_read(file_t* file, void* buffer, uint64_t count)
{
    shmem_t* shmem = file->private;
    uint64_t size = strlen(shmem->id) + 1;
    return BUFFER_READ(file, buffer, count, shmem->id, size);
}

static uint64_t shmem_seek(file_t* file, int64_t offset, seek_origin_t origin)
{
    shmem_t* shmem = file->private;
    uint64_t size = strlen(shmem->id) + 1;
    return BUFFER_SEEK(file, offset, origin, size);
}

static void shmem_vmm_callback(void* private)
{
    shmem_t* shmem = private;
    shmem_deref(shmem);
}

static void* shmem_mmap(file_t* file, void* address, uint64_t length, prot_t prot)
{
    shmem_t* shmem = file->private;
    LOCK_DEFER(&shmem->lock);

    process_t* process = sched_process();
    space_t* space = &process->space;

    uint64_t pageAmount = BYTES_TO_PAGES(length);
    if (pageAmount == 0)
    {
        return ERRPTR(EINVAL);
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
            shmem_ref(shmem));
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
            shmem_ref(shmem));
    }
}

static uint64_t shmem_open(inode_t* inode, file_t* file)
{
    shmem_t* shmem = file->private;
    file->private = shmem_ref(shmem);
    return 0;
}

static void shmem_cleanup(file_t* file)
{
    shmem_t* shmem = file->private;
    shmem_deref(shmem);
}

static file_ops_t normalOps = {
    .open = shmem_open,
    .read = shmem_read,
    .mmap = shmem_mmap,
    .cleanup = shmem_cleanup,
};

static uint64_t shmem_new_open(inode_t* inode, file_t* file)
{
    shmem_t* shmem = heap_alloc(sizeof(shmem_t), HEAP_NONE);
    if (shmem == NULL)
    {
        return ERR;
    }

    file->ops = &normalOps;
    file->private = shmem;

    atomic_init(&shmem->ref, 1);
    lock_init(&shmem->lock);
    ulltoa(atomic_fetch_add(&newId, 1), shmem->id, 10);
    shmem->segment = NULL;
    assert(sysobj_init(&shmem->obj, &dir, shmem->id, &normalOps, shmem) != ERR);
    return 0;
}

static file_ops_t newOps = {
    .open = shmem_new_open,
    .cleanup = shmem_cleanup,
};

void shmem_init(void)
{
    assert(sysdir_init(&dir, "/", "shmem", NULL) != ERR);
    assert(sysobj_init(&new, &dir, "new", &newOps, NULL) != ERR);
}
#include "shmem.h"

#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "sched/sched.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/atomint.h>

static atomic_uint64 newId = ATOMIC_VAR_INIT(0);

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
        free(shmem->segment);
    }
    free(shmem);
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

    uint64_t pageAmount = SIZE_IN_PAGES(length);
    if (pageAmount == 0)
    {
        return ERRPTR(EINVAL);
    }

    if (shmem->segment == NULL) // First call to mmap()
    {
        shmem_segment_t* segment = malloc(sizeof(shmem_segment_t) + sizeof(void*) * pageAmount);
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
                free(segment);
                return NULL;
            }
        }

        void* result = vmm_map_pages(address, segment->pages, segment->pageAmount, prot, shmem_vmm_callback, shmem_ref(shmem));
        if (result == NULL)
        {
            for (uint64_t i = 0; i < segment->pageAmount; i++)
            {
                pmm_free(segment->pages[i]);
            }
            free(segment);
            return NULL;
        }

        shmem->segment = segment;
        return result;
    }
    else
    {
        shmem_segment_t* segment = shmem->segment;

        return vmm_map_pages(address, segment->pages, segment->pageAmount, prot, shmem_vmm_callback, shmem_ref(shmem));
    }
}

static file_ops_t fileOps = (file_ops_t){
    .read = shmem_read,
    .mmap = shmem_mmap,
};

static file_t* shmem_open(volume_t* volume, const path_t* path, sysobj_t* sysobj)
{
    shmem_t* shmem = sysobj->private;
    
    file_t* file = file_new(volume, path, PATH_NONE);
    if (file == NULL)
    {
        return NULL;
    }
    file->ops = &fileOps;
    file->private = shmem_ref(shmem);
    return file;
}

static void shmem_cleanup(sysobj_t* sysobj, file_t* file)
{
    shmem_t* shmem = file->private;
    shmem_deref(shmem);
}

static sysobj_ops_t objOps = 
{
    .open = shmem_open,
    .cleanup = shmem_cleanup,
};

static file_t* shmem_new_open(volume_t* volume, const path_t* path, sysobj_t* sysobj)
{
    shmem_t* shmem = malloc(sizeof(shmem_t));
    if (shmem == NULL)
    {
        return NULL;
    }

    file_t* file = file_new(volume, path, PATH_NONE);
    if (file == NULL)
    {
        free(shmem);
        return NULL;
    }
    file->ops = &fileOps;
    file->private = shmem;

    atomic_init(&shmem->ref, 1);
    lock_init(&shmem->lock);
    ulltoa(atomic_fetch_add(&newId, 1), shmem->id, 10);
    shmem->segment = NULL;
    assert(sysobj_init(&shmem->obj, &dir, shmem->id, &objOps, shmem) != ERR);
    return file;
}

static sysobj_ops_t newOps =
{
    .open = shmem_new_open,
    .cleanup = shmem_cleanup,
};

void shmem_init(void)
{
    assert(sysdir_init(&dir, "/", "shmem", NULL) != ERR);
    assert(sysobj_init(&new, &dir, "new", &newOps, NULL) != ERR);
}
#pragma once

#include "fs/sysfs.h"
#include "sync/lock.h"

#include <sys/io.h>

typedef struct
{
    uint64_t pageAmount;
    void* pages[];
} shmem_segment_t;

typedef struct
{
    atomic_uint64_t ref;
    char id[MAX_NAME];
    sysfs_file_t obj;
    lock_t lock; // Lock only protects segment, other member are const, expect ref which is atomic.
    shmem_segment_t* segment;
} shmem_t;

void shmem_init(void);
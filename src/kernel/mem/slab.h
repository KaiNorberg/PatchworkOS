#pragma once

#include <assert.h>
#include <stdlib.h>
#include <sys/list.h>

#include "mem/pmm.h"
#include "sync/lock.h"

#define CACHE_MIN_LENGTH 16
#define CACHE_MAX_LENGTH 32

#define SLAB_MAX_EMPTY_CACHES 2

#define SLAB_MAGIC 0x3996609D

struct cache;
struct slab;

typedef struct
{
    list_entry_t entry;
    struct cache* cache;
    uint32_t magic;
    bool freed;
    uint64_t dataSize;
    uint8_t data[];
} object_t;

typedef struct cache
{
    list_entry_t entry;
    list_t freeList;
    struct slab* slab;
    uint64_t objectCount;
    uint64_t freeCount;
    uint8_t buffer[];
} cache_t;

typedef struct slab
{
    list_t emptyCaches;
    list_t partialCaches;
    list_t fullCaches;
    uint64_t emptyCacheCount;
    uint64_t objectSize;
    uint64_t optimalCacheSize;
} slab_t;

void slab_init(slab_t* slab, uint64_t objectSize);

object_t* slab_alloc(slab_t* slab);

void slab_free(slab_t* slab, object_t* object);

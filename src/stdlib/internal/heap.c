#include "heap.h"

#include <sys/io.h>
#include <sys/math.h>
#include <sys/proc.h>

#ifdef __EMBED__

#include "lock.h"
#include "pmm.h"
#include "vmm.h"

static lock_t lock;
static uintptr_t newAddress;

extern uint64_t _kernelEnd;

#else

static fd_t zeroResource;

#endif

static heap_header_t* firstBlock;

void _HeapBlockSplit(heap_header_t* block, uint64_t size)
{
    heap_header_t* newBlock = (heap_header_t*)((uint64_t)block + sizeof(heap_header_t) + size);
    newBlock->size = block->size - sizeof(heap_header_t) - size;
    newBlock->next = block->next;
    newBlock->reserved = false;
    newBlock->magic = HEAP_HEADER_MAGIC;

    block->size = size;
    block->next = newBlock;
}

heap_header_t* _HeapFirstBlock(void)
{
    return firstBlock;
}

#ifdef __EMBED__
heap_header_t* _HeapBlockNew(uint64_t size)
{
    uint64_t pageAmount = SIZE_IN_PAGES(size + sizeof(heap_header_t));

    heap_header_t* newBlock = (heap_header_t*)newAddress;
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        vmm_kernel_map((void*)(newAddress + i * PAGE_SIZE), VMM_HIGHER_TO_LOWER(pmm_alloc()), PAGE_SIZE);
    }
    newAddress += pageAmount * PAGE_SIZE;

    newBlock->size = pageAmount * PAGE_SIZE - sizeof(heap_header_t);
    newBlock->next = NULL;
    newBlock->reserved = false;
    newBlock->magic = HEAP_HEADER_MAGIC;

    return newBlock;
}

void _HeapInit(void)
{
    newAddress = ROUND_UP((uint64_t)&_kernelEnd, PAGE_SIZE);
    firstBlock = _HeapBlockNew(PAGE_SIZE);

    lock_init(&lock);
}

void _HeapAcquire(void)
{
    lock_acquire(&lock);
}

void _HeapRelease(void)
{
    lock_release(&lock);
}

#else

heap_header_t* _HeapBlockNew(uint64_t size)
{
    size = ROUND_UP(size + sizeof(heap_header_t), PAGE_SIZE);

    heap_header_t* newBlock = mmap(zeroResource, NULL, size, PROT_READ | PROT_WRITE);
    if (newBlock == NULL)
    {
        return NULL;
    }

    newBlock->size = size - sizeof(heap_header_t);
    newBlock->next = NULL;
    newBlock->reserved = false;
    newBlock->magic = HEAP_HEADER_MAGIC;

    return newBlock;
}

void _HeapInit(void)
{
    zeroResource = open("sys:/zero");
    firstBlock = _HeapBlockNew(PAGE_SIZE - sizeof(heap_header_t));
}

// TODO: Implement user space mutex
void _HeapAcquire(void)
{
}

void _HeapRelease(void)
{
}

#endif

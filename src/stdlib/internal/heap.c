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
    if (firstBlock == NULL)
    {
        firstBlock = _HeapBlockNew(PAGE_SIZE - sizeof(heap_header_t));
    }

    return firstBlock;
}

heap_header_t* _HeapBlockNew(uint64_t size)
{
    uint64_t pageAmount = SIZE_IN_PAGES(size + sizeof(heap_header_t));

    heap_header_t* newBlock = _PageAlloc(pageAmount);
    if (newBlock == NULL)
    {
        return NULL;
    }

    newBlock->size = pageAmount * PAGE_SIZE - sizeof(heap_header_t);
    newBlock->next = NULL;
    newBlock->reserved = false;
    newBlock->magic = HEAP_HEADER_MAGIC;

    return newBlock;
}

#ifdef __EMBED__

void _HeapInit(void)
{
    newAddress = ROUND_UP((uint64_t)&_kernelEnd, PAGE_SIZE);
    lock_init(&lock);

    firstBlock = NULL;
}

void _HeapAcquire(void)
{
    lock_acquire(&lock);
}

void _HeapRelease(void)
{
    lock_release(&lock);
}

void* _PageAlloc(uint64_t amount)
{
    void* addr = (void*)newAddress;
    if (vmm_kernel_alloc(addr, amount * PAGE_SIZE) == NULL)
    {
        return NULL;
    }
    newAddress += amount * PAGE_SIZE;

    return addr;
}

#else

void _HeapInit(void)
{
    zeroResource = open("sys:/zero");
    firstBlock = NULL;
}

// TODO: Implement user space mutex
void _HeapAcquire(void)
{
}

void _HeapRelease(void)
{
}

void* _PageAlloc(uint64_t amount)
{
    return mmap(zeroResource, NULL, amount * PAGE_SIZE, PROT_READ | PROT_WRITE);
}

#endif

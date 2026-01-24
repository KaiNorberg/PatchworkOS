#pragma once

#include <kernel/mem/paging_types.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/pool.h>
#include <kernel/mem/space.h>

#include <errno.h>
#include <stdlib.h>
#include <sys/list.h>

typedef struct process process_t;

/**
 * @brief Memory Descriptor List.
 * @defgroup kernel_mem_mdl Memory Descriptor List
 * @ingroup kernel_mem
 *
 * The Memory Descriptor List (MDL) is a structure used to describe non-contiguous physical memory, allowing it be
 * accessed as a single contiguous block regardless of the loaded address space.
 *
 * ## I/O Operations
 *
 * The MDL structure is primarily used to describe memory regions for I/O operations. For example, if a process
 * specifies a buffer to write to but that I/O operation is later completed while a different address space is loaded,
 * the kernel would be unable to access the buffer directly.
 *
 * Instead, the kernel can create an MDL for the buffer, which describes the physical memory pages backing that buffer,
 * allowing the I/O operation to be completed regardless of the currently loaded address space.
 *
 * @{
 */

/**
 * @brief Amount of memory segments statically allocated for small MDLs.
 */
#define MDL_SEGS_SMALL_MAX 2

/**
 * @brief Memory Descriptor List Segment structure.
 * @struct mdl_seg_t
 */
typedef struct mdl_seg
{
    pfn_t pfn;       ///< Page frame number.
    uint32_t size;   ///< Size of the segment in bytes.
    uint32_t offset; ///< Offset in bytes within the first page.
} mdl_seg_t;

/**
 * @brief Memory Descriptor List structure.
 * @struct mdl_t
 */
typedef struct mdl
{
    struct mdl* next;                    ///< Pointer to the next MDL.
    mdl_seg_t small[MDL_SEGS_SMALL_MAX]; ///< Statically allocated segments for small regions.
    mdl_seg_t* segments;                 ///< Pointer to segments array.
    uint32_t amount;                     ///< Number of memory segments.
    uint32_t capacity;                   ///< Capacity of the `large` array.
} mdl_t;

/**
 * @brief Initialize a Memory Descriptor List.
 *
 * @param next Pointer to the MDL.
 * @param prev Pointer to the previous MDL in the chain, or `NULL` if none.
 */
static inline void mdl_init(mdl_t* next, mdl_t* prev)
{
    if (prev != NULL)
    {
        prev->next = next;
    }
    next->next = NULL;
    next->segments = next->small;
    next->amount = 0;
    next->capacity = MDL_SEGS_SMALL_MAX;
}

/**
 * @brief Deinitialize a Memory Descriptor List.
 *
 * @param mdl Pointer to the MDL.
 */
void mdl_deinit(mdl_t* mdl);

/**
 * @brief Free a Memory Descriptor List chain.
 *
 * Will traverse the entire chain to deinitialize and free each MDL structure using the provided `free` function.
 *
 * @param mdl Pointer to the first MDL in the chain.
 * @param free Function to free the MDL structure itself, or `NULL` to only deinitialize.
 */
void mdl_free_chain(mdl_t* mdl, void (*free)(void*));

/**
 * @brief Initialize a Memory Descriptor List from a memory region.
 *
 * @param mdl Pointer to the MDL.
 * @param prev Pointer to the previous MDL in the chain, or `NULL` if none.
 * @param space The address space of the region.
 * @param addr The virtual address of the memory region.
 * @param size The size of the memory region in bytes.
 * @return On success, `EOK`. On failure, one of the following error codes:
 * - See `mdl_add()` for possible error codes.
 */
errno_t mdl_from_region(mdl_t* mdl, mdl_t* prev, space_t* space, const void* addr, size_t size);

/**
 * @brief Add a memory region to the Memory Descriptor List.
 *
 * @param mdl Pointer to the MDL.
 * @param space The address space of the user process.
 * @param addr The virtual address of the memory region.
 * @param size The size of the memory region in bytes.
 * @return On success, `EOK`. On failure, one of the following error codes:
 * - `EINVAL`: Invalid parameters.
 * - `ENOMEM`: Not enough memory to allocate segments.
 * - `EOVERFLOW`: The size specified is to large.
 * - `EFAULT`: Invalid address.
 */
errno_t mdl_add(mdl_t* mdl, space_t* space, const void* addr, size_t size);

/**
 * @brief Read from a Memory Descriptor List into a buffer.
 *
 * @param mdl The MDL to read from.
 * @param buffer The buffer to read into.
 * @param count Number of bytes to read.
 * @param offset Offset within the MDL to start reading from.
 * @return The number of bytes read.
 */
uint64_t mdl_read(mdl_t* mdl, void* buffer, size_t count, size_t offset);

/**
 * @brief Write to a Memory Descriptor List from a buffer.
 *
 * @param mdl The MDL to write to.
 * @param buffer The buffer to write from.
 * @param count Number of bytes to write.
 * @param offset Offset within the MDL to start writing to.
 * @return The number of bytes written.
 */
uint64_t mdl_write(mdl_t* mdl, const void* buffer, size_t count, size_t offset);

/**
 * @brief Memory Descriptor List Iterator structure.
 * @struct mdl_iter_t
 */
typedef struct
{
    mdl_t* mdl;
    size_t segIndex;
    size_t segOffset;
} mdl_iter_t;

/**
 * @brief Create a Memory Descriptor List Iterator initializer.
 *
 * @param _mdl Pointer to the MDL to iterate over.
 * @return MDL Iterator initializer.
 */
#define MDL_ITER_CREATE(_mdl) \
    { \
        .mdl = (_mdl), \
        .segIndex = 0, \
        .segOffset = 0, \
    }

/**
 * @brief Get the next byte from a Memory Descriptor List Iterator.
 *
 * @param iter Pointer to the MDL Iterator.
 * @param byte Pointer to store the retrieved byte.
 * @return `true` if a byte was retrieved, `false` if the end of the MDL was reached.
 */
static inline bool mdl_iter_next(mdl_iter_t* iter, uint8_t* byte)
{
    if (iter->segIndex >= iter->mdl->amount)
    {
        return false;
    }

    mdl_seg_t* seg = &iter->mdl->segments[iter->segIndex];
    uint8_t* addr = PFN_TO_VIRT(seg->pfn) + seg->offset + iter->segOffset;
    *byte = *(addr);

    iter->segOffset++;
    if (iter->segOffset >= seg->size)
    {
        iter->segIndex++;
        iter->segOffset = 0;
    }

    return true;
}

/**
 * @brief Iterate over bytes within a Memory Descriptor List.
 *
 * @param _byte The iterator variable.
 * @param _mdl Pointer to the MDL.
 */
#define MDL_FOR_EACH(_byte, _mdl) for (mdl_iter_t _iter = MDL_ITER_CREATE(_mdl); mdl_iter_next(&_iter, (_byte));)

/** @} */
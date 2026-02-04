#pragma once

#include <kernel/mem/paging_types.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/pool.h>
#include <kernel/mem/space.h>

#include <errno.h>
#include <stdlib.h>
#include <sys/list.h>
#include <sys/status.h>

typedef struct process process_t;

/**
 * @brief Memory Descriptor List.
 * @defgroup kernel_mem_mdl Memory Descriptor List
 * @ingroup kernel_mem
 *
 * The Memory Descriptor List (MDL) is a structure used to describe non-contiguous physical memory, allowing it be
 * accessed as a single contiguous block regardless of the loaded address space.
 *
 * ## Direct I/O
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
 * @return An appropriate status value.
 */
status_t mdl_from_region(mdl_t* mdl, mdl_t* prev, space_t* space, const void* addr, size_t size);

/**
 * @brief Add a memory region to the Memory Descriptor List.
 *
 * @param mdl Pointer to the MDL.
 * @param space The address space of the user process.
 * @param addr The virtual address of the memory region.
 * @param size The size of the memory region in bytes.
 * @return An appropriate status value.
 */
status_t mdl_add(mdl_t* mdl, space_t* space, const void* addr, size_t size);

/**
 * @brief Copy from a buffer into a Memory Descriptor List.
 *
 * @param mdl The MDL to copy into.
 * @param count Number of bytes to copy.
 * @param offset Pointer to the offset within the MDL to start copying to, will be updated.
 * @param bytesCopied Output pointer for the amount of bytes copied, can be `NULL`.
 * @param source The source buffer to copy from, can be `NULL` if `sourceLength == 0`.
 * @param sourceLength The maximum length of the source buffer.
 * @return An appropriate status value.
 */
status_t mdl_copy_from_buffer(mdl_t* mdl, size_t count, size_t* offset, size_t* bytesCopied, const void* source,
    size_t sourceLength);

/**
 * @brief Copy to a buffer from a Memory Descriptor List.
 *
 * @param mdl The MDL to copy from.
 * @param count Number of bytes to copy.
 * @param offset Pointer to the offset within the MDL to start copying to, will be updated.
 * @param bytesCopied Output pointer for the amount of bytes copied, can be `NULL`.
 * @param dest The destination buffer to copy to.
 * @param destLength The maximum length of the destination buffer.
 * @return An appropriate status value.
 */
status_t mdl_copy_to_buffer(mdl_t* mdl, size_t count, size_t* offset, size_t* bytesCopied, void* dest, size_t destLength);

/**
 * @brief Copy from a circular buffer into a Memory Descriptor List.
 *
 * @param mdl The MDL to copy into.
 * @param count Number of bytes to copy.
 * @param offset Pointer to the offset within the MDL to start copying to, will be updated.
 * @param bytesCopied Output pointer for the amount of bytes copied, can be `NULL`.
 * @param src The source circular buffer.
 * @param srcLen The size of the circular buffer.
 * @param srcIndex The monotonic index to start copying from in the circular buffer.
 * @return An appropriate status value.
 */
status_t mdl_copy_from_circular(mdl_t* mdl, size_t count, size_t* offset, size_t* bytesCopied, const void* src,
    size_t srcLen, size_t srcIndex);

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
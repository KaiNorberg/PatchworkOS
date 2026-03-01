#pragma once

#include <kernel/mem/paging_types.h>
#include <kernel/mem/pmm.h>

#include <errno.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/status.h>

typedef struct process process_t;
typedef struct space space_t;

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
 * specifies a buffer for a write operation to but that operation is later completed while a different address space is
 * loaded, the kernel would be unable to access the buffer.
 *
 * Instead, the kernel can create an MDL for the buffer, which describes the physical memory pages backing that buffer,
 * allowing the operation to be completed regardless of the currently loaded address space.
 *
 * This is especially powerfull as we already identity map all physical memory to the higher half of the address space,
 * as such we dont need to map the physical pages but can access them directly.
 *
 * ## Chains
 *
 * MDLs can be chained together by storing a pointer to the next MDL in the `mdl_t::next` field.
 *
 * This is primarily used by the IRP system to save a little bit of memory, and does not serve much purpose beyond that
 * as a single MDL is already capable of "Scatter Gather I/O".
 *
 * @{
 */

/**
 * @brief Amount of memory descriptors statically allocated for small MDLs.
 *
 * @note By setting this value to atleast 2, we ensure that any contiguous virtual buffer that is less than or equal to
 * a page in size can be described without any dynamic allocation.
 */
#define MDL_SMALL_MAX 2

/**
 * @brief Memory Descriptor List entry structure.
 * @struct mdl_entry_t
 *
 * Each entry only describes a single physical memory page, if a buffer crosses a page boundary an additional
 * descriptor will be needed.
 */
typedef struct mdl_entry
{
    pfn_t pfn;       ///< Page frame number.
    uint32_t size;   ///< Size of the region within the page.
    uint32_t offset; ///< Offset in bytes within the page.
} mdl_entry_t;

/**
 * @brief Memory Descriptor List structure.
 * @struct mdl_t
 */
typedef struct mdl
{
    struct mdl* next;                 ///< Pointer to the next MDL.
    mdl_entry_t small[MDL_SMALL_MAX]; ///< Statically allocated entry for simple regions.
    mdl_entry_t* entries;             ///< Pointer to entries array.
    uint32_t amount;                  ///< Number of entries.
    uint32_t capacity;                ///< Capacity of the `large` array.
    size_t size;                      ///< Total size of the memory region described by the MDL.
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
    next->entries = next->small;
    next->amount = 0;
    next->capacity = MDL_SMALL_MAX;
    next->size = 0;
}

/**
 * @brief Deinitialize a Memory Descriptor List.
 *
 * @param mdl Pointer to the MDL.
 */
void mdl_deinit(mdl_t* mdl);

/**
 * @brief Get the size of a Memory Descriptor List.
 *
 * @param mdl Pointer to the MDL.
 * @return The total size of the MDL in bytes.
 */
static inline size_t mdl_size(mdl_t* mdl)
{
    return mdl->size;
}

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
 * @brief Add an array of I/O vectors to the Memory Descriptor List.
 *
 * @note This function will function safely even if the vectors are in user-memory or not in the currently loaded
 * address space.
 *
 * @param mdl Pointer to the MDL.
 * @param space The address space of the user process.
 * @param vector Pointer to the array of I/O vectors, can be `NULL` if `count == 0`.
 * @param count The number of vectors in the array.
 * @return An appropriate status value.
 */
status_t mdl_add_vector(mdl_t* mdl, space_t* space, const iovec_t* vector, size_t count);

/**
 * @brief Copy from a buffer into a Memory Descriptor List.
 *
 * @param mdl The MDL to copy into.
 * @param count Number of bytes to copy.
 * @param offset The offset within the MDL to start copying to.
 * @param copied Output pointer for the amount of bytes copied, can be `NULL`.
 * @param source The source buffer to copy from, can be `NULL` if `sourceLength == 0`.
 * @param sourceLength The maximum length of the source buffer.
 * @return An appropriate status value.
 */
status_t mdl_copy_in(mdl_t* mdl, size_t count, size_t offset, size_t* copied, const void* source, size_t sourceLength);

/**
 * @brief Copy to a buffer from a Memory Descriptor List.
 *
 * @param mdl The MDL to copy from.
 * @param count Number of bytes to copy.
 * @param offset The offset within the MDL to start copying from.
 * @param copied Output pointer for the amount of bytes copied, can be `NULL`.
 * @param dest The destination buffer to copy to.
 * @param destLength The maximum length of the destination buffer.
 * @return An appropriate status value.
 */
status_t mdl_copy_out(mdl_t* mdl, size_t count, size_t offset, size_t* copied, void* dest, size_t destLength);

/**
 * @brief Set all bytes within a Memory Descriptor List to a specific value.
 *
 * @param mdl The MDL to fill.
 * @param count Number of bytes to fill.
 * @param offset The offset within the MDL to start filling from.
 * @param filled Output pointer for the amount of bytes filled, can be `NULL`.
 * @param value The value to fill with.
 * @return An appropriate status value.
 */
status_t mdl_fill(mdl_t* mdl, size_t count, size_t offset, size_t* filled, uint8_t value);

/**
 * @brief Memory Descriptor List Iterator structure.
 * @struct mdl_iter_t
 */
typedef struct
{
    mdl_t* mdl;
    size_t entryIndex;
    size_t entryOffset;
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
        .entryIndex = 0, \
        .entryOffset = 0, \
    }

/**
 * @brief Get the next byte from a Memory Descriptor List Iterator.
 *
 * @param iter Pointer to the MDL Iterator.
 * @param ptr Pointer to store the address of the retrieved byte.
 * @return `true` if a byte was retrieved, `false` if the end of the MDL was reached.
 */
static inline bool mdl_iter_next(mdl_iter_t* iter, void** ptr)
{
    if (iter->entryIndex >= iter->mdl->amount)
    {
        return false;
    }

    mdl_entry_t* entry = &iter->mdl->entries[iter->entryIndex];
    uint8_t* addr = PFN_TO_VIRT(entry->pfn) + entry->offset + iter->entryOffset;
    *ptr = addr;

    iter->entryOffset++;
    if (iter->entryOffset >= entry->size)
    {
        iter->entryIndex++;
        iter->entryOffset = 0;
    }

    return true;
}

/**
 * @brief Iterate over bytes within a Memory Descriptor List.
 *
 * @param _ptr The iterator variable.
 * @param _mdl Pointer to the MDL.
 */
#define MDL_FOR_EACH(_ptr, _mdl) for (mdl_iter_t _iter = MDL_ITER_CREATE(_mdl); mdl_iter_next(&_iter, (void**)&(_ptr));)

/** @} */
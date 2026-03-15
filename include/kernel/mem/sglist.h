#pragma once

#include <kernel/mem/pagevec.h>
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
 * @brief Scatter-Gather List.
 * @defgroup kernel_mem_sglist Scatter-Gather List
 * @ingroup kernel_mem
 *
 * The Scatter-Gather List (sglist) is a structure used to describe non-contiguous physical memory, allowing it be
 * accessed as a single contiguous block regardless of the loaded address space.
 *
 * ## Direct I/O
 *
 * The sglist structure is primarily used to describe memory regions for I/O operations. For example, if a process
 * specifies a buffer for a write operation to but that operation is later completed while a different address space is
 * loaded, the kernel would be unable to access the buffer.
 *
 * Instead, the kernel can create an sglist for the buffer, which describes the physical memory pages backing that
 * buffer, allowing the operation to be completed regardless of the currently loaded address space.
 *
 * This is especially powerfull as we already identity map all physical memory to the higher half of the address space,
 * as such we dont need to map the physical pages but can access them directly.
 *
 * ## Chains
 *
 * Sglists can be chained together by storing a pointer to the next sglist in the `sglist_t::next` field.
 *
 * This is primarily used by the IRP system to save a little bit of memory, and does not serve much purpose beyond that.
 *
 * @{
 */

/**
 * @brief Amount of memory descriptors statically allocated for small sglists.
 *
 * @note By setting this value to atleast 2, we ensure that any contiguous virtual buffer that is less than or equal to
 * a page in size can be described without any dynamic allocation.
 */
#define SGLIST_SMALL_MAX 2

/**
 * @brief Scatter-Gather List entry structure.
 * @struct sglist_entry_t
 *
 * Each entry only describes a single physical memory page, if a buffer crosses a page boundary an additional
 * descriptor will be needed.
 */
typedef struct sglist_entry
{
    pfn_t pfn;       ///< Page frame number.
    uint32_t size;   ///< Size of the region within the page.
    uint32_t offset; ///< Offset in bytes within the page.
} sglist_entry_t;

/**
 * @brief Scatter-Gather List structure.
 * @struct sglist_t
 */
typedef struct sglist
{
    struct sglist* next;                    ///< Pointer to the next sglist.
    sglist_entry_t small[SGLIST_SMALL_MAX]; ///< Statically allocated entry for simple regions.
    sglist_entry_t* entries;                ///< Pointer to entries array.
    uint32_t amount;                        ///< Number of entries.
    uint32_t capacity;                      ///< Capacity of the `large` array.
    size_t size;                            ///< Total size of the memory region described by the sglist.
} sglist_t;

/**
 * @brief Initialize a Scatter-Gather List.
 *
 * @param next Pointer to the sglist.
 * @param prev Pointer to the previous sglist in the chain, or `NULL` if none.
 */
static inline void sglist_init(sglist_t* next, sglist_t* prev)
{
    if (prev != NULL)
    {
        prev->next = next;
    }
    next->next = NULL;
    next->entries = next->small;
    next->amount = 0;
    next->capacity = SGLIST_SMALL_MAX;
    next->size = 0;
}

/**
 * @brief Deinitialize a Scatter-Gather List.
 *
 * @param list Pointer to the sglist.
 */
void sglist_deinit(sglist_t* list);

/**
 * @brief Get the size of a Scatter-Gather List.
 *
 * @param list Pointer to the sglist.
 * @return The total size of the sglist in bytes.
 */
static inline size_t sglist_size(sglist_t* list)
{
    return list->size;
}

/**
 * @brief Free a Scatter-Gather List chain.
 *
 * Will traverse the entire chain to deinitialize and free each sglist structure using the provided `free` function.
 *
 * @param list Pointer to the first sglist in the chain.
 * @param free Function to free the sglist structure itself, or `NULL` to only deinitialize.
 */
void sglist_free_chain(sglist_t* list, void (*free)(void*));

/**
 * @brief Add a memory region to the Scatter-Gather List.
 *
 * @param list Pointer to the sglist.
 * @param space The address space of the user process.
 * @param addr The virtual address of the memory region.
 * @param size The size of the memory region in bytes.
 * @return An appropriate status value.
 */
status_t sglist_add(sglist_t* list, space_t* space, const void* addr, size_t size);

/**
 * @brief Add an array of I/O vectors to the Scatter-Gather List.
 *
 * @note This function will function safely even if the vectors are in user-memory or not in the currently loaded
 * address space.
 *
 * @param list Pointer to the sglist.
 * @param space The address space of the user process.
 * @param vector Pointer to the array of I/O vectors, can be `NULL` if `count == 0`.
 * @param count The number of vectors in the array.
 * @return An appropriate status value.
 */
status_t sglist_add_vector(sglist_t* list, space_t* space, const iovec_t* vector, size_t count);

/**
 * @brief Copy from a buffer into a Scatter-Gather List.
 *
 * @param list The sglist to copy into.
 * @param count Number of bytes to copy.
 * @param offset The offset within the sglist to start copying to.
 * @param copied Output pointer for the amount of bytes copied, can be `NULL`.
 * @param source The source buffer to copy from, can be `NULL` if `sourceLength == 0`.
 * @param sourceLength The maximum length of the source buffer.
 * @return An appropriate status value.
 */
status_t sglist_copy_in(sglist_t* list, size_t count, size_t offset, size_t* copied, const void* source,
    size_t sourceLength);

/**
 * @brief Copy to a buffer from a Scatter-Gather List.
 *
 * @param list The sglist to copy from.
 * @param count Number of bytes to copy.
 * @param offset The offset within the sglist to start copying from.
 * @param copied Output pointer for the amount of bytes copied, can be `NULL`.
 * @param dest The destination buffer to copy to.
 * @param destLength The maximum length of the destination buffer.
 * @return An appropriate status value.
 */
status_t sglist_copy_out(sglist_t* list, size_t count, size_t offset, size_t* copied, void* dest, size_t destLength);

/**
 * @brief Set all bytes within a Scatter-Gather List to a specific value.
 *
 * @param list The sglist to fill.
 * @param count Number of bytes to fill.
 * @param offset The offset within the sglist to start filling from.
 * @param filled Output pointer for the amount of bytes filled, can be `NULL`.
 * @param value The value to fill with.
 * @return An appropriate status value.
 */
status_t sglist_fill(sglist_t* list, size_t count, size_t offset, size_t* filled, uint8_t value);

/**
 * @brief Copy from a page vector into a Scatter-Gather List.
 *
 * @param list The sglist to copy into.
 * @param count Number of bytes to copy.
 * @param listOffset The offset within the sglist to start copying to.
 * @param copied Output pointer for the amount of bytes copied, can be `NULL`.
 * @param vec The source page vector to copy from.
 * @param vecOffset The offset within the page vector to start copying from.
 * @return An appropriate status value.
 */
status_t sglist_copy_in_pagevec(sglist_t* list, size_t count, size_t listOffset, size_t* copied, const pagevec_t* vec,
    size_t vecOffset);

/**
 * @brief Copy to a page vector from a Scatter-Gather List.
 *
 * @param list The sglist to copy from.
 * @param count Number of bytes to copy.
 * @param listOffset The offset within the sglist to start copying from.
 * @param copied Output pointer for the amount of bytes copied, can be `NULL`.
 * @param vec The destination page vector to copy to.
 * @param vecOffset The offset within the page vector to start copying to.
 * @return An appropriate status value.
 */
status_t sglist_copy_out_pagevec(sglist_t* list, size_t count, size_t listOffset, size_t* copied, pagevec_t* vec,
    size_t vecOffset);

/**
 * @brief Copy from a virtual address in a specific address space into a Scatter-Gather List.
 *
 * @param list The sglist to copy into.
 * @param count Number of bytes to copy.
 * @param listOffset The offset within the sglist to start copying to.
 * @param copied Output pointer for the amount of bytes copied, can be `NULL`.
 * @param space The address space to read from.
 * @param source The virtual address in the address space to read from.
 * @return An appropriate status value.
 */
status_t sglist_copy_in_space(sglist_t* list, size_t count, size_t listOffset, size_t* copied, space_t* space,
    const void* source);

/**
 * @brief Copy to a virtual address in a specific address space from a Scatter-Gather List.
 *
 * @param list The sglist to copy from.
 * @param count Number of bytes to copy.
 * @param listOffset The offset within the sglist to start copying from.
 * @param copied Output pointer for the amount of bytes copied, can be `NULL`.
 * @param space The address space to write to.
 * @param dest The virtual address in the address space to write to.
 * @return An appropriate status value.
 */
status_t sglist_copy_out_space(sglist_t* list, size_t count, size_t listOffset, size_t* copied, space_t* space,
    void* dest);

/**
 * @brief Scatter-Gather List Iterator structure.
 * @struct sglist_iter_t
 */
typedef struct
{
    sglist_t* list;
    size_t entryIndex;
    size_t entryOffset;
} sglist_iter_t;

/**
 * @brief Create a Scatter-Gather List Iterator initializer.
 *
 * @param _list Pointer to the sglist to iterate over.
 * @return sglist Iterator initializer.
 */
#define SGLIST_ITER_CREATE(_list) \
    { \
        .list = (_list), \
        .entryIndex = 0, \
        .entryOffset = 0, \
    }

/**
 * @brief Get the next byte from a Scatter-Gather List Iterator.
 *
 * @param iter Pointer to the sglist Iterator.
 * @param ptr Pointer to store the address of the retrieved byte.
 * @return `true` if a byte was retrieved, `false` if the end of the sglist was reached.
 */
static inline bool sglist_iter_next(sglist_iter_t* iter, void** ptr)
{
    if (iter->entryIndex >= iter->list->amount)
    {
        return false;
    }

    sglist_entry_t* entry = &iter->list->entries[iter->entryIndex];
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
 * @brief Iterate over bytes within a Scatter-Gather List.
 *
 * @param _ptr The iterator variable.
 * @param _list Pointer to the sglist.
 */
#define SGLIST_FOR_EACH(_ptr, _list) \
    for (sglist_iter_t _iter = SGLIST_ITER_CREATE(_list); sglist_iter_next(&_iter, (void**)&(_ptr));)

/** @} */
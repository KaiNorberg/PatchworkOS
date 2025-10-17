#pragma once

#include <assert.h>
#include <stdint.h>
#include <sys/math.h>

/**
 * @brief Paging
 * @defgroup common_paging Paging
 * @ingroup common
 *
 * Paging is used to map virtual memory to physical memory. Meaning that when some address is accessed by the CPU, the
 * address might actually point to a different location in physical memory. This is done using a page table.
 *
 * Additonally Patchwork uses page tables to store metadata about memory pages which is then used by the vmm to avoid
 * the need for a seperate data structure that keeps track of memory.
 *
 * For this implementation it was decided to try to derive every value from first principles, meaning that no values are
 * hardcoded, for example the lower and higher half bounderies are derived from the number of bits that can be used for
 * the address in a page table entry. This makes the code more complex, but means we rely on fewer potentially incorrect
 * sources.
 *
 * @see [OSDev Paging](https://wiki.osdev.org/Paging)
 *
 * @{
 */

/**
 * @brief One entry in a page table.
 * @typedef pml_entry_t
 */
typedef struct
{
    union {
        uint64_t raw;
        struct
        {
            uint64_t present : 1;       ///< If set the page is present in memory and readable.
            uint64_t write : 1;         ///< If set the page is writable.
            uint64_t user : 1;          ///< If set the page is accessible from user mode.
            uint64_t writeThrough : 1;  ///< If set write-through caching is enabled for the page.
            uint64_t cacheDisabled : 1; ///< If set caching is disabled for the page.
            uint64_t accessed : 1;      ///< If set the page has been accessed (read or written to).
            uint64_t dirty : 1;         ///< If set the page has been written to.
            uint64_t size : 1;          // Unused
            uint64_t global : 1;        ///< If set the page is not flushed from the TLB on a context switch.
            /**
             * If set, then when the entry is unmapped or the page table is freed, the physical page will be freed.
             * (Defined by PatchworkOS)
             */
            uint64_t owned : 1;
            uint64_t available1 : 2; ///< Unused bits available for OS use.
            uint64_t addr : 40; ///< The address contained in the entry, note that this is shifted right by 12 bits.
            /**
             * Check the virtual memory manager for more information. (Defined by PatchworkOS)
             */
            uint64_t callbackId : 7;
            uint64_t protection : 4;
            uint64_t noExecute : 1;
        };
    };
} pml_entry_t;

/**
 * @brief Number of bits used for the offset within a page.
 *
 * Each page is 4KB (2^12 bytes).
 */
#define PML_ADDR_OFFSET_BITS 12

/**
 * @brief Mask for the address in a page table entry.
 *
 * The address is stored in bits 12-51 of the entry.
 */
#define PML_ADDR_MASK 0x000FFFFFFFFFF000ULL

/**
 * @brief A entry in a page table without a specified address or callback ID.
 * @struct pml_flags_t
 *
 * Used to simplify setting or changing flags in a page table entry.
 */
typedef enum
{
    PML_NONE = 0,
    PML_PRESENT = (1ULL << 0),
    PML_WRITE = (1ULL << 1),
    PML_USER = (1ULL << 2),
    PML_WRITE_THROUGH = (1ULL << 3),
    PML_CACHE_DISABLED = (1ULL << 4),
    PML_ACCESSED = (1ULL << 5),
    PML_DIRTY = (1ULL << 6),
    PML_SIZE = (1ULL << 7),
    PML_GLOBAL = (1ULL << 8),
    PML_OWNED = (1ULL << 9),
    PML_AVAILABLE1 = (1ULL << 10),
    PML_AVAILABLE2 = (1ULL << 11),
    PML_NO_EXECUTE = (1ULL << 63),
} pml_flags_t;

/**
 * @brief Mask for all pml flags.
 */
#define PML_FLAGS_MASK \
    (PML_PRESENT | PML_WRITE | PML_USER | PML_WRITE_THROUGH | PML_CACHE_DISABLED | PML_ACCESSED | PML_DIRTY | \
        PML_SIZE | PML_GLOBAL | PML_OWNED | PML_AVAILABLE1 | PML_AVAILABLE2 | PML_NO_EXECUTE)

/**
 * @brief Enums for the different page table levels.
 * @enum pml_level_t
 *
 * A page table is a tree like structure with 4 levels, each level has 512 entries.
 * The levels are named PML1 (or PT), PML2 (or PD), PML3 (or PDPT) and PML4.
 *
 * The PML4 is the root of the tree and each entry in the PML4 points to a PML3, each entry in a PML3 points to a PML2,
 * and each entry in a PML2 points to a PML1. Each entry in a PML1 points to a 4KB page in memory.
 *
 * The tree is traversed such that given some input virtual address, we calculate the index into each level using
 * `PML_ADDR_TO_INDEX()`, until we reach the PML1 level, which then, as mentioned, points to the actual page in
 * memory. So, in short, input virtual memory, traverse down the tree to find the physical memory and its flags.
 */
typedef enum
{
    PML1 = 1,
    PT = 1, ///< Page Table
    PML2 = 2,
    PD = 2, ///< Page Directory
    PML3 = 3,
    PDPT = 3, ///< Page Directory Pointer Table
    PML4 = 4,
    PML_LEVEL_AMOUNT = 4, ///< Total number of levels in the page table.
} pml_level_t;

/**
 * @brief Indexes into a pml level.
 *
 * In each pml entry there are 512 entries, the first 256 entries map the lower half of the address space,
 * the last 256 entries map the higher half of the address space.
 *
 * For each half of the entries the address mapped by it increases by a set amount depending on the level in the tree
 * structure, but for the higher half the addresses wraps around and instead the address is or'd by 0xFFFF800000000000.
 *
 * @see PML_INDEX_TO_ADDR() for the wrapping behavior.
 */
typedef enum
{
    PML_INDEX_LOWER_HALF_MIN = 0,
    PML_INDEX_LOWER_HALF_MAX = 255,
    PML_INDEX_HIGHER_HALF_MIN = 256,
    PML_INDEX_HIGHER_HALF_MAX = 511,
    PML_INDEX_AMOUNT = 512,
    PML_INDEX_INVALID = 512
} pml_index_t;

/**
 * @brief Number of bits used to index into a page table level.
 *
 * Each page table level has 512 entries (2^9 = 512).
 */
#define PML_INDEX_BITS 9

/**
 * @brief Calculates the lowest virtual address that maps to a given index at a specified page table level, without
 * wrapping.
 *
 * This macro does not handle the wrapping behavior for the higher half of the address space, use `PML_INDEX_TO_ADDR()`
 * instead.
 *
 * @param index The index within the page table level.
 * @param level The page table level.
 * @return The lowest virtual addr that maps to the given index at the specified level
 */
#define PML_INDEX_TO_ADDR_NO_WRAP(index, level) \
    ((uintptr_t)(index) << (((level) - 1) * PML_INDEX_BITS + PML_ADDR_OFFSET_BITS))

/**
 * @brief Total number of bits used for virtual addresses.
 *
 * The x86_64 architecture only uses 48 bits for virtual addresses.
 * However, with 5 level paging (not used here) it would be possible to use up to 57 bits.
 *
 * PatchworkOS currently uses 4 level paging, so we have:
 * - 9 bits for PML4
 * - 9 bits for PML3
 * - 9 bits for PML2
 * - 9 bits for PML1
 * - 12 bits for page offset
 *
 * Total: 48 bits.
 */
#define PML_VIRT_ADDR_BITS (PML_INDEX_BITS * PML_LEVEL_AMOUNT + PML_ADDR_OFFSET_BITS)

/**
 * @brief The start of the lower half of the address space.
 *
 * The lower half starts at address 0. Obviously.
 */
#define PML_LOWER_HALF_START (0)

/**
 * @brief The end of the lower half of the address space.
 *
 * Can be thought of as the last page-aligned address before bit `PML_VIRT_ADDR_BITS - 1` is set.
 */
#define PML_LOWER_HALF_END ((1ULL << (PML_VIRT_ADDR_BITS - 1)) - PAGE_SIZE)

/**
 * @brief The start of the higher half of the address space.
 *
 * Note that the "gap" between the lower half and higher half is con-canonical and thus invalid to access.
 *
 * Calculated by sign-extending the bit `PML_VIRT_ADDR_BITS - 1`.
 */
#define PML_HIGHER_HALF_START (~((1ULL << (PML_VIRT_ADDR_BITS - 1)) - 1))

/**
 * @brief The end of the higher half of the address space.
 *
 * Calculated by taking the maximum possible address (all bits set) and aligning it down to the nearest page boundary.
 */
#define PML_HIGHER_HALF_END (~0ULL & ~(PAGE_SIZE - 1))

/**
 * @brief Converts an address from the higher half to the lower half.
 *
 * @param addr The address to convert.
 * @return The converted address.
 */
#define PML_HIGHER_TO_LOWER(addr) ((uintptr_t)(addr) - PML_HIGHER_HALF_START)

/**
 * @brief Converts an address from the lower half to the higher half.
 *
 * @param addr The address to convert.
 * @return The converted address.
 */
#define PML_LOWER_TO_HIGHER(addr) ((uintptr_t)(addr) + PML_HIGHER_HALF_START)

/**
 * @brief Ensures that the given address is in the lower half of the address space.
 *
 * If the address is in the higher half, it is converted to the lower half.
 * If the address is already in the lower half, it is returned unchanged.
 *
 * @param addr The address to ensure is in the lower half.
 * @return The address in the lower half of the address space.
 */
#define PML_ENSURE_LOWER_HALF(addr) \
    ((uintptr_t)(addr) >= PML_HIGHER_HALF_START ? PML_HIGHER_TO_LOWER(addr) : (uintptr_t)(addr))

/**
 * @brief Calculates the index into a page table level for a given virtual address.
 *
 * @param addr The virtual address.
 * @param level The page table level.
 * @return The index within the page table level.
 */
#define PML_ADDR_TO_INDEX(addr, level) \
    ((pml_index_t)(((uintptr_t)(addr) >> (((level) - 1) * PML_INDEX_BITS + PML_ADDR_OFFSET_BITS)) & \
        (PML_INDEX_AMOUNT - 1)))

/**
 * @brief Calculates the lowest virtual address that maps to a given index at a specified page table level.
 *
 * @param index The index within the page table level.
 * @param level The page table level.
 * @return The lowest virtual addr that maps to the given index at the specified level
 */
#define PML_INDEX_TO_ADDR(index, level) \
    ((uintptr_t)(index) < PML_INDEX_HIGHER_HALF_MIN \
            ? PML_INDEX_TO_ADDR_NO_WRAP(index, level) \
            : (PML_INDEX_TO_ADDR_NO_WRAP(index, level) | PML_HIGHER_HALF_START))

/**
 * @brief Maximum number of callbacks that can be registered for a page table.
 *
 * This is limited by the number of bits available in the page table entry for storing the callback ID.
 */
#define PML_MAX_CALLBACK (1 << 7)

/**
 * @brief Special callback ID that indicates no callback is associated with the page.
 */
#define PML_CALLBACK_NONE (1 << 7)

/**
 * @brief Callback ID type.
 */
typedef uint8_t pml_callback_id_t;

/**
 * @brief A page table level.
 * @struct pml_t
 */
typedef struct
{
    pml_entry_t entries[PML_INDEX_AMOUNT];
} pml_t;

/**
 * @brief Generic page allocation function type.
 *
 * Used to allow both the kernel and bootloader to provide their own page allocation functions.
 */
typedef void* (*pml_alloc_page_t)(void);

/**
 * @brief Generic page free function type.
 *
 * Used to allow both the kernel and bootloader to provide their own page free functions.
 */
typedef void (*pml_free_page_t)(void*);

/**
 * @brief A page table structure.
 * @struct page_table_t
 *
 * The `page_table_t` structure represents a page table, which stores the root of the page table (PML4) and
 * function pointers for allocating and freeing pages. The `pml4` pointer is whats actually loaded into the CR3 register
 * to switch page tables.
 */
typedef struct page_table
{
    pml_alloc_page_t allocPage;
    pml_free_page_t freePage;
    pml_t* pml4;
} page_table_t;

/** @} */

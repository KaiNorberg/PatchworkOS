#pragma once

#include <assert.h>
#include <stdint.h>
#include <sys/math.h>

/**
 * @brief Page Tables.
 * @defgroup common_pml PML
 * @ingroup common
 *
 * The `pml.h` file stores functions for manipulating page tables.
 *
 * @{
 */

/**
 * @brief Page table entry flags.
 */
typedef enum
{
    PML_NONE = 0,
    PML_PRESENT = (1 << 0),
    PML_WRITE = (1 << 1),
    PML_USER = (1 << 2),
    PML_WRITE_THROUGH = (1 << 3),
    PML_CACHE_DISABLED = (1 << 4),
    PML_ACCESSED = (1 << 5),
    PML_DIRTY = (1 << 6),
    PML_SIZE = (1 << 7),
    PML_GLOBAL = (1 << 8),
    PML_OWNED = (1 << 9),
    PML_INHERIT = (1 << 10),
} pml_flags_t;

/**
 * @brief Number of entries in a page table.
 */
#define PML_ENTRY_AMOUNT 512

/**
 * @brief Maximum addressable memory by the page tables, rounded down to the nearest page.
 */
#define PML_HIGHER_HALF_END (UINTPTR_MAX - 0xFFF)

/**
 * @brief Start of the higher half of the address space.
 */
#define PML_HIGHER_HALF_START 0xFFFF800000000000

/**
 * @brief End of the lower half of the address space, rounded down to the nearest page.
 */
#define PML_LOWER_HALF_END 0x7FFFFFFFF000

/**
 * @brief The lowest address that we use in the lower half, used to catch null pointer dereferences and similar bugs.
 */
#define PML_LOWER_HALF_START 0x400000

/**
 * @brief Converts an address from the higher half to the lower half.
 */
#define PML_HIGHER_TO_LOWER(address) ((void*)((uint64_t)(address) - PML_HIGHER_HALF_START))

/**
 * @brief Converts an address from the lower half to the higher half.
 */
#define PML_LOWER_TO_HIGHER(address) ((void*)((uint64_t)(address) + PML_HIGHER_HALF_START))

/**
 * @brief Ensures that the given address is in the lower half of the address space.
 *
 * If the address is in the higher half, it is converted to the lower half.
 * If the address is already in the lower half, it is returned unchanged.
 */
#define PML_ENSURE_LOWER_HALF(address) \
    (address >= (void*)PML_HIGHER_HALF_START ? PML_HIGHER_TO_LOWER(address) : address)

/**
 * @brief Extracts the index in a page table level from a virtual address.
 */
#define PML_GET_INDEX(address, level) (((uint64_t)(address) >> (((level) - 1) * 9 + 12)) & 0x1FF)

/**
 * @brief Maximum number of callbacks that can be registered for a space.
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
 * @brief One entry in a page table.
 * @struct pml_entry_t
 */
typedef union {
    uint64_t raw;
    struct
    {
        uint64_t present : 1;
        uint64_t write : 1;
        uint64_t user : 1;
        uint64_t writeThrough : 1;
        uint64_t cacheDisabled : 1;
        uint64_t accessed : 1;
        uint64_t dirty : 1;
        uint64_t pageSize : 1;
        uint64_t global : 1;
        /**
         * Defined by Patchwork. Specifies if the page is owned by the page pml4 and should be freed when the
         * page is unmapped.
         */
        uint64_t owned : 1;
        /**
         * Defined by Patchwork. Specifies if a parent space should inherit this mapping when a new space is created.
         *
         * Really just used to make sure the kernel is inherited by all spaces.
         */
        uint64_t inherit : 1;
        uint64_t available : 1;
        uint64_t address : 40;
        /**
         * Defined by Patchwork. Specifies the id of the associated callback of the page.
         */
        uint64_t callbackId : 7;
        uint64_t protection : 4;
        uint64_t noExecute : 1;
    };
} pml_entry_t;

/**
 * @brief A page table level.
 * @struct pml_t
 */
typedef struct
{
    pml_entry_t entries[PML_ENTRY_AMOUNT];
} pml_t;

/**
 * @brief Generic page allocation function type.
 */
typedef void* (*pml_alloc_page_t)(void);

/**
 * @brief Generic page free function type.
 */
typedef void (*pml_free_page_t)(void*);

/**
 * @brief A page table structure.
 * @struct page_table_t
 */
typedef struct page_table
{
    pml_t* pml4;
    pml_alloc_page_t allocPage;
    pml_free_page_t freePage;
} page_table_t;

/** @} */

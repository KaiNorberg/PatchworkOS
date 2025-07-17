#pragma once

#include "cpu/regs.h"
#include "defs.h"
#include "mem/pmm.h"

#include <assert.h>
#include <sys/math.h>

/**
 * @brief Page Tables.
 * @defgroup common_pml PML
 * @ingroup common
 *
 * The `pml.h` file stores functions for manipulating page tables.
 *
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
    PML_OWNED = (1 << 9)
} pml_flags_t;

#define PML_ENTRY_AMOUNT 512

#define PML_HIGHER_HALF_END (UINTPTR_MAX - 0xFFF)
#define PML_HIGHER_HALF_START 0xFFFF800000000000
#define PML_LOWER_HALF_END (0x7FFFFFFFF000)
#define PML_LOWER_HALF_START 0x400000

#define PML_HIGHER_TO_LOWER(address) ((void*)((uint64_t)(address) - PML_HIGHER_HALF_START))
#define PML_LOWER_TO_HIGHER(address) ((void*)((uint64_t)(address) + PML_HIGHER_HALF_START))

// Allows a address to be specified in either the upper or lower half
#define PML_ENSURE_LOWER_HALF(address) \
    (address >= (void*)PML_HIGHER_HALF_START ? PML_HIGHER_TO_LOWER(address) : address)

#define PML_GET_INDEX(address, level) (((uint64_t)(address) >> (((level) - 1) * 9 + 12)) & 0x1FF)

#define PML_MAX_CALLBACK (1 << 7)
#define PML_CALLBACK_NONE (1 << 7)

typedef uint8_t pml_callback_id_t;

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
         * @brief Defined by Patchwork. Specifies if the page is owned by the page pml4 and should be freed when the
         * page is unmapped.
         */
        uint64_t owned : 1;
        uint64_t available1 : 2;
        uint64_t address : 40;
        /**
         * @brief Defined by Patchwork. Specifies the id of the associated callback of the page.
         */
        uint64_t callbackId : 7;
        uint64_t protection : 4;
        uint64_t noExecute : 1;
    };
} pml_entry_t;

typedef struct
{
    pml_entry_t entries[PML_ENTRY_AMOUNT];
} pml_t;

typedef void* (*pml_alloc_page_t)(void);
typedef void (*pml_free_page_t)(void*);

typedef struct page_table
{
    pml_t* pml4;
    pml_alloc_page_t allocPage;
    pml_free_page_t freePage;
} page_table_t;

#pragma once

#include <kernel/mem/pmm.h>
#include <kernel/mem/pool.h>

#include <sys/list.h>

typedef struct process process_t;

/**
 * @brief Memory Descriptor.
 * @defgroup kernel_mem_mem_desc Memory Descriptor
 * @ingroup kernel_mem
 *
 * @{
 */

typedef struct mem_seg
{
    pfn_t page;
    uint32_t length;
    uint32_t offset;
} mem_seg_t;

typedef struct ALIGNED(64) mem_desc
{

} mem_desc_t;

/** @} */
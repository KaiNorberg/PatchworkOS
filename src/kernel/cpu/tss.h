#pragma once

#include "stack_pointer.h"

#include <common/defs.h>
#include <stdint.h>

/**
 * @brief Task State Segment
 * @defgroup kernel_cpu_tss TSS
 * @ingroup kernel_cpu
 *
 * The Task State Segment is more or less deprecated, we use it only to tell the cpu what stack pointer to load when
 * switching from userspace to kernelspace.
 *
 * @see [OSDev Wiki TTS](https://wiki.osdev.org/Task_State_Segment)
 */

/**
 * @brief Interrupt Stack Table indices.
 */
typedef enum
{
    TSS_IST_NONE = 0,
    TSS_IST1 = 1,
    TSS_IST2 = 2,
    TSS_IST3 = 3,
    TSS_IST4 = 4,
    TSS_IST5 = 5,
    TSS_IST6 = 6,
    TSS_IST7 = 7,
    TSS_IST_COUNT = 7
} tss_ist_t;

/**
 * @brief The IST index to use for exceptions.
 */
#define TSS_IST_EXCEPTION TSS_IST1

/**
 * @brief The IST index to use for double faults.
 */
#define TSS_IST_DOUBLE_FAULT TSS_IST2

/**
 * @brief Task State Segment structure.
 * @struct tss_t
 *
 * The stack pointers `rsp*` store the stack to use when switching to a higher privilege level. Additionally we use the
 * ists to specify additional stacks for handling exceptions. All exceptions will use `TSS_IST1` except for double
 * faults which will use `TSS_IST2`.
 *
 * So this means that when a double fault occurs the CPU switches to the stack in `ist[1]`, if any other exception
 * occurs the CPU switches to the stack in `ist[0]`, and if any other ring transition occurs (for example an interrupt
 * from userspace to kernelspace) the CPU switches to the stack in `rsp0`. Otherwise the current stack is used.
 *
 */
typedef struct PACKED
{
    uint32_t reserved1;
    uint64_t rsp0; ///< Stack pointer to load when switching to ring 0.
    uint64_t rsp1; ///< Stack pointer to load when switching to ring 1, unused.
    uint64_t rsp2; ///< Stack pointer to load when switching to ring 2, unused.
    uint64_t reserved2;
    uint64_t ist[7]; // Interrupt Stack Table.
    uint64_t reserved3;
    uint16_t reserved4;
    uint16_t iopb; ///< Offset to the I/O permission bitmap, we set this to the size of the TSS to disable the bitmap.
} tss_t;

/**
 * @brief Load the TSS.
 *
 * Loads the TSS using the `ltr` instruction, the TSS must already be present in the GDT.
 */
extern void tss_load(void);

/**
 * @brief Initialize a TSS structure.
 *
 * @param tss The TSS structure to initialize.
 */
void tss_init(tss_t* tss);

/**
 * @brief Load a kernel stack into the TTS.
 *
 * Sets all the `rsp*` registers to the provided stack.
 *
 * @param tss The TSS structure to load the stack pointers into.
 * @param stack The stack to load into the TSS.
 */
void tss_kernel_stack_load(tss_t* tss, stack_pointer_t* stack);

/**
 * @brief Load a stack into an IST entry.
 *
 * @param tss The TSS structure to load the IST into.
 * @param ist The IST index to load the stack into.
 * @param stack The stack to load into the TSS.
 */
void tss_ist_load(tss_t* tss, tss_ist_t ist, stack_pointer_t* stack);

/** @} */

#pragma once

#include "stack_pointer.h"

#include <common/defs.h>
#include <stdint.h>

/**
 * @brief Task State Segment
 * @defgroup kernel_cpu_tss TSS
 * @ingroup kernel_cpu
 *
 * The Task State Segment is more or less deprecated, we use it only to tell the cpu what stack pointer to use when
 * handling interrupts. This is done using the Interrupt Stack Table (IST).
 *
 * @see [OSDev Wiki TTS](https://wiki.osdev.org/Task_State_Segment)
 *
 * @{
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
 * @brief The IST index to use for other interrupts.
 */
#define TSS_IST_INTERRUPT TSS_IST3

/**
 * @brief Task State Segment structure.
 * @struct tss_t
 *
 * The stack pointers `rsp*` store the stack to use when switching to a higher privilege level which we do not use.
 * Instead we have a total of 4 stacks used while in kernel space, 3 per-cpu stacks and 1 per-thread stack. Of course
 * there is also the user stack used while in user space.
 *
 * The per-cpu stacks are:
 * - Exception stack, used while handling exceptions, specified in ist[0].
 * - Double fault stack, used while handling double faults, specified in ist[1].
 * - Interrupt stack, used while handling all other interrupts, specified in ist[2].
 *
 * The per-thread stack is the kernel stack, used while the thread is in kernel space and NOT handling an exception or
 * interrupt. In effect this is used in system calls, boot, inital thread loading and if the thread is a kernel thread
 * it is used all the time. This stack is not handled by the TSS but instead by the system call and scheduler code.
 *
 * The way the interrupt stack table works is that when a interrupt occurs the cpu checks the IDT gate for that
 * interrupt, if it has a non zero IST index it will then load that stack pointer from the TSS and switch to that stack,
 * this happens regardless of the current privilege level.
 *
 */
typedef struct PACKED
{
    uint32_t reserved1;
    uint64_t rsp0; ///< Stack pointer to load when switching to ring 0, unused.
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
 * @brief Load a stack into an IST entry.
 *
 * @param tss The TSS structure to load the IST into.
 * @param ist The IST index to load the stack into.
 * @param stack The stack to load into the TSS.
 */
void tss_ist_load(tss_t* tss, tss_ist_t ist, stack_pointer_t* stack);

/** @} */

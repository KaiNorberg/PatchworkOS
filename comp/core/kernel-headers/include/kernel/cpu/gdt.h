#pragma once

#ifndef __ASSEMBLER__
#include <kernel/cpu/tss.h>
#include <sys/defs.h>

#include <stdint.h>
#endif

/**
 * @brief Global Descriptor Table
 * @defgroup kernel_cpu_gdt GDT
 * @ingroup kernel_cpu
 *
 * The global descriptor table is a legacy feature from the days of 32-bit x86 and older. Most of its features are
 * unused, but is still required in 64-bit mode to specify the current privilage level and to load the TSS.
 *
 * @see [OSDev Wiki GDT](https://wiki.osdev.org/Global_Descriptor_Table)
 *
 * @{
 */

#define GDT_RING0 0 ///< Kernel ring.
#define GDT_RING1 1 ///< Unused ring.
#define GDT_RING2 2 ///< Unused ring.
#define GDT_RING3 3 ///< User ring.

#define GDT_NULL 0           ///< Null segment selector, unused but the gdt must start with it.
#define GDT_KERNEL_CODE 0x08 ///< Kernel code segment selector.
#define GDT_KERNEL_DATA 0x10 ///< Kernel data segment selector.
#define GDT_USER_DATA 0x18   ///< User data segment selector.
#define GDT_USER_CODE 0x20   ///< User code segment selector.
#define GDT_TSS 0x28         ///< TSS segment selector.

#define GDT_CS_RING0 (GDT_KERNEL_CODE | GDT_RING0) ///< Value to load into the CS register for kernel code.
#define GDT_SS_RING0 (GDT_KERNEL_DATA | GDT_RING0) ///< Value to load into the SS register for kernel data.

#define GDT_CS_RING3 (GDT_USER_CODE | GDT_RING3) ///< Value to load into the CS register for user code.
#define GDT_SS_RING3 (GDT_USER_DATA | GDT_RING3) ///< Value to load into the SS register for user data.

#define GDT_ACCESS_ACCESSED (1 << 0) ///< Will be set to 1 when accessed, but its best to set it to 1 manually.
#define GDT_ACCESS_READ_WRITE \
    (1 << 1) ///< If set on a code segment, its readable. If set on a data segment, its writable.
#define GDT_ACCESS_DIRECTION_CONFORMING \
    (1 << 2) ///< If set on a data segment, the segment grows downwards. If set on a code segment, conforming.
#define GDT_ACCESS_EXEC (1 << 3)      ///< If set, the segment is a code segment. If clear, its a data segment.
#define GDT_ACCESS_SYSTEM (0 << 4)    ///< Is a system segment.
#define GDT_ACCESS_DATA_CODE (1 << 4) ///< Is a data or code segment.

#define GDT_ACCESS_TYPE_LDT 0x2           ///< Local Descriptor Table. Only used if the SYSTEM bit is 0.
#define GDT_ACCESS_TYPE_TSS_AVAILABLE 0x9 ///< Available 64-bit Task State Segment. Only used if the SYSTEM bit is 0.
#define GDT_ACCESS_TYPE_TSS_BUSY 0xB      ///< Busy 64-bit Task State Segment. Only used if the SYSTEM bit is 0.

#define GDT_ACCESS_RING0 (0 << 5) ///< Descriptor Privilege Level 0 (kernel).
#define GDT_ACCESS_RING1 (1 << 5) ///< Descriptor Privilege Level 1, unused.
#define GDT_ACCESS_RING2 (2 << 5) ///< Descriptor Privilege Level 2, unused.
#define GDT_ACCESS_RING3 (3 << 5) ///< Descriptor Privilege Level 3 (user).

#define GDT_ACCESS_PRESENT (1 << 7) ///< Must be 1 for all valid segments.

#define GDT_FLAGS_NONE 0             ///< No flags set.
#define GDT_FLAGS_RESERVED (1 << 0)  ///< Must be 0.
#define GDT_FLAGS_LONG_MODE (1 << 1) ///< If set, its a 64-bit code segment.
#define GDT_FLAGS_SIZE \
    (1 << 2) ///< If set, its a 32-bit segment. If clear, its a 16-bit segment. Ignored in 64-bit mode.
#define GDT_FLAGS_4KB (1 << 3) ///< If set, the limit is in 4KiB blocks. If clear, the limit is in bytes.

#ifndef __ASSEMBLER__

/**
 * @brief GDT descriptor structure.
 *
 * Used to load the GDT with the `lgdt` instruction.
 */
typedef struct PACKED
{
    uint16_t size;   ///< Size of the GDT - 1.
    uint64_t offset; ///< Address of the GDT.
} gdt_desc_t;

/**
 * @brief A single GDT segment descriptor.
 * @struct gdt_segment_t
 *
 * This stucture is the same for both 32-bit and 64-bit mode.
 */
typedef struct PACKED
{
    uint16_t limitLow;
    uint16_t baseLow;
    uint8_t baseMiddle;
    uint8_t access;
    uint8_t flagsAndLimitHigh;
    uint8_t baseHigh;
} gdt_segment_t;

/**
 * @brief A long mode system segment descriptor, used for TSS and LDT.
 * @struct gdt_long_system_segment_t
 *
 * This structure is used for 64-bit TSS and LDT descriptors.
 */
typedef struct PACKED
{
    uint16_t limitLow;
    uint16_t baseLow;
    uint8_t baseLowerMiddle;
    uint8_t access;
    uint8_t flagsAndLimitHigh;
    uint8_t baseUpperMiddle;
    uint32_t baseHigh;
    uint32_t reserved;
} gdt_long_system_segment_t;

/**
 * @brief The actual GDT structure
 * @struct gdt_t
 *
 * Note that we actually only need one TTS descriptor, not one per cpu, as its only used while loading a TTS, after that
 * its just useless.
 */
typedef struct PACKED
{
    gdt_segment_t null;
    gdt_segment_t kernelCode;
    gdt_segment_t kernelData;
    gdt_segment_t userData;
    gdt_segment_t userCode;
    gdt_long_system_segment_t tssDesc;
} gdt_t;

/**
 * @brief Loads a GDT descriptor.
 *
 * Dont use this directly use `gdt_cpu_load()` instead.
 *
 * @param descriptor The GDT descriptor to load.
 */
extern void gdt_load_descriptor(gdt_desc_t* descriptor);

/**
 * @brief Initialize the GDT.
 *
 * This will setup the GDT structure in memory, but will not load it. Loading is done in `gdt_cpu_load()`.
 */
void gdt_init(void);

/**
 * @brief Load the GDT on the current CPU.
 *
 * This will load the GDT using the `lgdt` instruction and then reload all segment registers to use the new GDT.
 *
 * Must be called after `gdt_init()`.
 */
void gdt_cpu_load(void);

/**
 * @brief Load a TSS into the GDT and load it using the `ltr` instruction on the current CPU.
 *
 * Note that the actual TTS descriptor in the GDT can be shared between CPUs, as its only used while loading the TSS,
 * after that its just useless.
 *
 * @param tss The TSS to load.
 */
void gdt_cpu_tss_load(tss_t* tss);

#endif // __ASSEMBLER__

/** @} */

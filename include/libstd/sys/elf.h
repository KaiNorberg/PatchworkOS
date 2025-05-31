#ifndef _SYS_ELF_H
#define _SYS_ELF_H 1

#include <stdint.h>

/**
 * @brief Executable and linkable format definitions.
 * @ingroup libstd
 * @defgroup libstd_sys_elf Executable and linkable file format
 *
 * The `sys/elf.h` header defines structs and constants for ELF files used in Patchwork, note that Patchwork only
 * supports ELF files.
 *
 */

typedef uint16_t elf_hdr_type_t;
#define ELF_HDR_TYPE_NONE 0x0
#define ELF_HDR_TYPE_REL 0x1
#define ELF_HDR_TYPE_EXEC 0x2
#define ELF_HDR_TYPE_DYN 0x3
#define ELF_HDR_TYPE_CORE 0x4
#define ELF_HDR_TYPE_LOW_PROC 0xff00
#define ELF_HDR_TYPE_HIGH_PROC 0xffff

typedef uint16_t elf_hdr_machine_t;
#define ELF_HDR_MACHINE_NONE 0
#define ELF_HDR_MACHINE_X86_64 0x3E

typedef uint32_t elf_hdr_version_t;
#define ELF_HDR_VERSION_1 1

typedef uint32_t elf_phdr_type_t;
#define ELF_PHDR_TYPE_NULL 0
#define ELF_PHDR_TYPE_LOAD 1
#define ELF_PHDR_TYPE_DYNAMIC 2
#define ELF_PHDR_TYPE_INTERP 3
#define ELF_PHDR_TYPE_NOTE 4
#define ELF_PHDR_TYPE_SHLIB 5
#define ELF_PHDR_TYPE_PHDR 6
#define ELF_PHDR_TYPE_TLS 7
#define ELF_PHDR_TYPE_LOW_OS 0x60000000
#define ELF_PHDR_TYPE_HIGH_OS 0x6fffffff
#define ELF_PHDR_TYPE_LOW_PROC 0x70000000
#define ELF_PHDR_TYPE_HIGH_PROC 0x7fffffff
// TODO: Decide if these are going to be kept from linux or not.
#define ELF_PHDR_TYPE_GNU_EH_FRAME (ELF_PHDR_TYPE_LOW_OS + 0x474e550)
#define ELF_PHDR_TYPE_GNU_STACK (ELF_PHDR_TYPE_LOW_OS + 0x474e551)
#define ELF_PHDR_TYPE_GNU_RELRO (ELF_PHDR_TYPE_LOW_OS + 0x474e552)
#define ELF_PHDR_TYPE_GNU_PROPERTY (ELF_PHDR_TYPE_LOW_OS + 0x474e553)

typedef uint32_t elf_phdr_flags_t;
#define ELF_PHDR_FLAGS_EXECUTE (1 << 0)
#define ELF_PHDR_FLAGS_WRITE (1 << 1)
#define ELF_PHDR_FLAGS_READ (1 << 2)

/**
 * @brief Checks the validity of an ELF header.
 * @ingroup libstd_sys_elf
 *
 * The `ELF_IS_VALID()` macro checks that the ELF file header is using version 1, 64-bit, x86_64 with little endian and
 * System V ABI.
 *
 * @param hdr A pointer to the ELF header structure.
 * @return True if the ELF header is valid, false otherwise.
 */
#define ELF_IS_VALID(hdr) \
    ((hdr)->ident[0] == 0x7F && (hdr)->ident[1] == 'E' && (hdr)->ident[2] == 'L' && (hdr)->ident[3] == 'F' && \
        (hdr)->ident[4] == 2 && (hdr)->ident[5] == 1 && (hdr)->ident[7] == 0 && \
        (hdr)->machine == ELF_HDR_MACHINE_X86_64 && (hdr)->version == ELF_HDR_VERSION_1)

/**
 * @brief ELF file header.
 * @ingroup libstd_sys_elf
 *
 * The `elf_hdr_t` structure stored att the begining of elf files.
 *
 */
typedef struct
{
    uint8_t ident[16];
    elf_hdr_type_t type;
    elf_hdr_machine_t machine;
    elf_hdr_version_t version;
    uint64_t entry;
    uint64_t phdrOffset;
    uint64_t shdrOffset;
    uint32_t flags;
    uint16_t headerSize;
    uint16_t phdrSize;
    uint16_t phdrAmount;
    uint16_t shdrSize;
    uint16_t shdrAmount;
    uint16_t shdrStringIndex;
} elf_hdr_t;

/**
 * @brief ELF program header.
 * @ingroup libstd_sys_elf
 *
 * The `elf_phdr_t` structure used in ELF files to store program sections (eg,. text, data, etc).
 *
 */
typedef struct
{
    elf_phdr_type_t type;
    elf_phdr_flags_t flags;
    uint64_t offset;
    uint64_t virtAddr;
    uint64_t physAddr;
    uint64_t fileSize;
    uint64_t memorySize;
    uint64_t align;
} elf_phdr_t;

/**
 * @brief ELF section header.
 * @ingroup libstd_sys_elf
 *
 * The `elf_shdr_t` structure used in ELF files to store information about a section.
 *
 */
typedef struct
{
    uint32_t name;
    uint32_t type;
    uint64_t flags;
    uint64_t address;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t addressAlign;
    uint64_t entrySize;
} elf_shdr_t;

#endif

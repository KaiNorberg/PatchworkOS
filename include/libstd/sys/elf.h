#ifndef _SYS_ELF_H
#define _SYS_ELF_H 1

#include <stdint.h>

/**
 * @brief Executable and linkable format definitions.
 * @ingroup libstd
 * @defgroup libstd_sys_elf ELF file
 *
 * The `sys/elf.h` header defines structs and constants for ELF files used in Patchwork, note that Patchwork only
 * supports ELF files.
 *
 * @see https://wiki.osdev.org/ELF for more information about the ELF format.
 *
 * @{
 */

/* ELF Identification indices */
#define ELF_IDENT_MAG0 0
#define ELF_IDENT_MAG1 1
#define ELF_IDENT_MAG2 2
#define ELF_IDENT_MAG3 3
#define ELF_IDENT_CLASS 4
#define ELF_IDENT_DATA 5
#define ELF_IDENT_VERSION 6
#define ELF_IDENT_OSABI 7
#define ELF_IDENT_ABIVERSION 8
#define ELF_IDENT_PAD 9

/* ELF Magic number */
#define ELF_MAG0 0x7F
#define ELF_MAG1 'E'
#define ELF_MAG2 'L'
#define ELF_MAG3 'F'

/* ELF Class */
#define ELF_CLASS_NONE 0
#define ELF_CLASS_32 1
#define ELF_CLASS_64 2

/* ELF Data encoding */
#define ELF_DATA_NONE 0
#define ELF_DATA_2LSB 1  /* Little endian */
#define ELF_DATA_2MSB 2  /* Big endian */

/* ELF Version */
#define ELF_VERSION_NONE 0
#define ELF_VERSION_CURRENT 1

/* ELF OS/ABI */
#define ELF_OSABI_SYSV 0
#define ELF_OSABI_HPUX 1
#define ELF_OSABI_NETBSD 2
#define ELF_OSABI_LINUX 3
#define ELF_OSABI_SOLARIS 6
#define ELF_OSABI_AIX 7
#define ELF_OSABI_IRIX 8
#define ELF_OSABI_FREEBSD 9
#define ELF_OSABI_TRU64 10
#define ELF_OSABI_MODESTO 11
#define ELF_OSABI_OPENBSD 12
#define ELF_OSABI_ARM_AEABI 64
#define ELF_OSABI_ARM 97
#define ELF_OSABI_STANDALONE 255

/* ELF File types */
typedef uint16_t elf_hdr_type_t;
#define ELF_HDR_TYPE_NONE 0x0
#define ELF_HDR_TYPE_REL 0x1
#define ELF_HDR_TYPE_EXEC 0x2
#define ELF_HDR_TYPE_DYN 0x3
#define ELF_HDR_TYPE_CORE 0x4
#define ELF_HDR_TYPE_LOW_PROC 0xff00
#define ELF_HDR_TYPE_HIGH_PROC 0xffff

/* ELF Machine types */
typedef uint16_t elf_hdr_machine_t;
#define ELF_HDR_MACHINE_NONE 0x00
#define ELF_HDR_MACHINE_M32 0x01
#define ELF_HDR_MACHINE_SPARC 0x02
#define ELF_HDR_MACHINE_386 0x03
#define ELF_HDR_MACHINE_68K 0x04
#define ELF_HDR_MACHINE_88K 0x05
#define ELF_HDR_MACHINE_860 0x07
#define ELF_HDR_MACHINE_MIPS 0x08
#define ELF_HDR_MACHINE_PARISC 0x0F
#define ELF_HDR_MACHINE_SPARC32PLUS 0x12
#define ELF_HDR_MACHINE_PPC 0x14
#define ELF_HDR_MACHINE_PPC64 0x15
#define ELF_HDR_MACHINE_S390 0x16
#define ELF_HDR_MACHINE_ARM 0x28
#define ELF_HDR_MACHINE_SH 0x2A
#define ELF_HDR_MACHINE_SPARCV9 0x2B
#define ELF_HDR_MACHINE_IA64 0x32
#define ELF_HDR_MACHINE_X86_64 0x3E
#define ELF_HDR_MACHINE_AARCH64 0xB7
#define ELF_HDR_MACHINE_RISCV 0xF3

/* ELF Version */
typedef uint32_t elf_hdr_version_t;
#define ELF_HDR_VERSION_NONE 0
#define ELF_HDR_VERSION_1 1

/* ELF Program header types */
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
/* GNU Extensions */
#define ELF_PHDR_TYPE_GNU_EH_FRAME (ELF_PHDR_TYPE_LOW_OS + 0x474e550)
#define ELF_PHDR_TYPE_GNU_STACK (ELF_PHDR_TYPE_LOW_OS + 0x474e551)
#define ELF_PHDR_TYPE_GNU_RELRO (ELF_PHDR_TYPE_LOW_OS + 0x474e552)
#define ELF_PHDR_TYPE_GNU_PROPERTY (ELF_PHDR_TYPE_LOW_OS + 0x474e553)

/* ELF Program header flags */
typedef uint32_t elf_phdr_flags_t;
#define ELF_PHDR_FLAGS_EXECUTE (1 << 0)
#define ELF_PHDR_FLAGS_WRITE (1 << 1)
#define ELF_PHDR_FLAGS_READ (1 << 2)

/* ELF Section header types */
typedef uint32_t elf_shdr_type_t;
#define ELF_SHDR_TYPE_NULL 0
#define ELF_SHDR_TYPE_PROGBITS 1
#define ELF_SHDR_TYPE_SYMTAB 2
#define ELF_SHDR_TYPE_STRTAB 3
#define ELF_SHDR_TYPE_RELA 4
#define ELF_SHDR_TYPE_HASH 5
#define ELF_SHDR_TYPE_DYNAMIC 6
#define ELF_SHDR_TYPE_NOTE 7
#define ELF_SHDR_TYPE_NOBITS 8
#define ELF_SHDR_TYPE_REL 9
#define ELF_SHDR_TYPE_SHLIB 10
#define ELF_SHDR_TYPE_DYNSYM 11
#define ELF_SHDR_TYPE_INIT_ARRAY 14
#define ELF_SHDR_TYPE_FINI_ARRAY 15
#define ELF_SHDR_TYPE_PREINIT_ARRAY 16
#define ELF_SHDR_TYPE_GROUP 17
#define ELF_SHDR_TYPE_SYMTAB_SHNDX 18
#define ELF_SHDR_TYPE_LOW_OS 0x60000000
#define ELF_SHDR_TYPE_HIGH_OS 0x6fffffff
#define ELF_SHDR_TYPE_LOW_PROC 0x70000000
#define ELF_SHDR_TYPE_HIGH_PROC 0x7fffffff
#define ELF_SHDR_TYPE_LOW_USER 0x80000000
#define ELF_SHDR_TYPE_HIGH_USER 0x8fffffff

/* ELF Section header flags */
typedef uint64_t elf_shdr_flags_t;
#define ELF_SHDR_FLAGS_WRITE (1 << 0)
#define ELF_SHDR_FLAGS_ALLOC (1 << 1)
#define ELF_SHDR_FLAGS_EXECINSTR (1 << 2)
#define ELF_SHDR_FLAGS_MERGE (1 << 4)
#define ELF_SHDR_FLAGS_STRINGS (1 << 5)
#define ELF_SHDR_FLAGS_INFO_LINK (1 << 6)
#define ELF_SHDR_FLAGS_LINK_ORDER (1 << 7)
#define ELF_SHDR_FLAGS_OS_NONCONFORMING (1 << 8)
#define ELF_SHDR_FLAGS_GROUP (1 << 9)
#define ELF_SHDR_FLAGS_TLS (1 << 10)
#define ELF_SHDR_FLAGS_COMPRESSED (1 << 11)
#define ELF_SHDR_FLAGS_MASKOS 0x0ff00000
#define ELF_SHDR_FLAGS_MASKPROC 0xf0000000

/* Special section indices */
#define ELF_SHN_UNDEF 0
#define ELF_SHN_LORESERVE 0xff00
#define ELF_SHN_LOPROC 0xff00
#define ELF_SHN_HIPROC 0xff1f
#define ELF_SHN_LOOS 0xff20
#define ELF_SHN_HIOS 0xff3f
#define ELF_SHN_ABS 0xfff1
#define ELF_SHN_COMMON 0xfff2
#define ELF_SHN_XINDEX 0xffff
#define ELF_SHN_HIRESERVE 0xffff

/* Symbol table entry info */
#define ELF_ST_BIND(info) ((info) >> 4)
#define ELF_ST_TYPE(info) ((info) & 0xf)
#define ELF_ST_INFO(bind, type) (((bind) << 4) + ((type) & 0xf))

/* Symbol binding */
#define ELF_STB_LOCAL 0
#define ELF_STB_GLOBAL 1
#define ELF_STB_WEAK 2
#define ELF_STB_LOOS 10
#define ELF_STB_HIOS 12
#define ELF_STB_LOPROC 13
#define ELF_STB_HIPROC 15

/* Symbol types */
#define ELF_STT_NOTYPE 0
#define ELF_STT_OBJECT 1
#define ELF_STT_FUNC 2
#define ELF_STT_SECTION 3
#define ELF_STT_FILE 4
#define ELF_STT_COMMON 5
#define ELF_STT_TLS 6
#define ELF_STT_LOOS 10
#define ELF_STT_HIOS 12
#define ELF_STT_LOPROC 13
#define ELF_STT_HIPROC 15

/* Dynamic entry tags */
#define ELF_DT_NULL 0
#define ELF_DT_NEEDED 1
#define ELF_DT_PLTRELSZ 2
#define ELF_DT_PLTGOT 3
#define ELF_DT_HASH 4
#define ELF_DT_STRTAB 5
#define ELF_DT_SYMTAB 6
#define ELF_DT_RELA 7
#define ELF_DT_RELASZ 8
#define ELF_DT_RELAENT 9
#define ELF_DT_STRSZ 10
#define ELF_DT_SYMENT 11
#define ELF_DT_INIT 12
#define ELF_DT_FINI 13
#define ELF_DT_SONAME 14
#define ELF_DT_RPATH 15
#define ELF_DT_SYMBOLIC 16
#define ELF_DT_REL 17
#define ELF_DT_RELSZ 18
#define ELF_DT_RELENT 19
#define ELF_DT_PLTREL 20
#define ELF_DT_DEBUG 21
#define ELF_DT_TEXTREL 22
#define ELF_DT_JMPREL 23
#define ELF_DT_BIND_NOW 24
#define ELF_DT_INIT_ARRAY 25
#define ELF_DT_FINI_ARRAY 26
#define ELF_DT_INIT_ARRAYSZ 27
#define ELF_DT_FINI_ARRAYSZ 28
#define ELF_DT_RUNPATH 29
#define ELF_DT_FLAGS 30
#define ELF_DT_ENCODING 32
#define ELF_DT_PREINIT_ARRAY 32
#define ELF_DT_PREINIT_ARRAYSZ 33
#define ELF_DT_SYMTAB_SHNDX 34
#define ELF_DT_LOW_OS 0x6000000d
#define ELF_DT_HIGH_OS 0x6ffff000
#define ELF_DT_LOW_PROC 0x70000000
#define ELF_DT_HIGH_PROC 0x7fffffff

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
    ((hdr)->ident[ELF_IDENT_MAG0] == ELF_MAG0 && (hdr)->ident[ELF_IDENT_MAG1] == ELF_MAG1 && \
        (hdr)->ident[ELF_IDENT_MAG2] == ELF_MAG2 && (hdr)->ident[ELF_IDENT_MAG3] == ELF_MAG3 && \
        (hdr)->ident[ELF_IDENT_CLASS] == ELF_CLASS_64 && (hdr)->ident[ELF_IDENT_DATA] == ELF_DATA_2LSB && \
        (hdr)->ident[ELF_IDENT_VERSION] == ELF_VERSION_CURRENT && (hdr)->ident[ELF_IDENT_OSABI] == ELF_OSABI_SYSV && \
        (hdr)->machine == ELF_HDR_MACHINE_X86_64 && (hdr)->version == ELF_HDR_VERSION_1)

/**
 * @brief ELF file header.
 *
 * The `elf_hdr_t` structure stored at the beginning of ELF files.
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
 *
 * The `elf_phdr_t` structure used in ELF files to store program sections (e.g., text, data, etc).
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
 *
 * The `elf_shdr_t` structure used in ELF files to store information about a section.
 */
typedef struct
{
    uint32_t name;
    elf_shdr_type_t type;
    elf_shdr_flags_t flags;
    uint64_t address;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t addressAlign;
    uint64_t entrySize;
} elf_shdr_t;

/**
 * @brief ELF symbol table entry.
 *
 * The `elf_sym_t` structure used to represent symbols in ELF files.
 */
typedef struct
{
    uint32_t name;
    uint8_t info;
    uint8_t other;
    uint16_t shndx;
    uint64_t value;
    uint64_t size;
} elf_sym_t;

/**
 * @brief ELF relocation entry with addend.
 *
 * The `elf_rela_t` structure used for relocation entries that include an addend.
 */
typedef struct
{
    uint64_t offset;
    uint64_t info;
    int64_t addend;
} elf_rela_t;

/**
 * @brief ELF relocation entry without addend.
 *
 * The `elf_rel_t` structure used for relocation entries without an addend.
 */
typedef struct
{
    uint64_t offset;
    uint64_t info;
} elf_rel_t;

/**
 * @brief ELF dynamic entry.
 *
 * The `elf_dyn_t` structure used in the dynamic section for dynamic linking information.
 */
typedef struct
{
    int64_t tag;
    union
    {
        uint64_t val;
        uint64_t ptr;
    } un;
} elf_dyn_t;

/**
 * @brief ELF note header.
 *
 * The `elf_note_t` structure used for note sections.
 */
typedef struct
{
    uint32_t nameSize;
    uint32_t descSize;
    uint32_t type;
} elf_note_t;

#endif

/** @} */

#pragma once

#include <stdint.h>

#define ET_NONE 0x00
#define ET_REL 0x01
#define ET_EXEC 0x02
#define ET_DYN 0x03
#define ET_CORE 0x04

#define EM_X86_64 0x3E

#define PT_LOAD 0x1

#define PF_EXECUTE (1 << 0)
#define PF_WRITE (1 << 1)
#define PF_READ (1 << 2)

typedef struct
{
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t programHeaderOffset;
    uint64_t sectionHeaderOffset;
    uint32_t flags;
    uint16_t headerSize;
    uint16_t programHeaderSize;
    uint16_t programHeaderAmount;
    uint16_t sectionHeaderSize;
    uint16_t sectionHeaderAmount;
    uint16_t sectionHeaderStringIndex;
} ElfHeader;

typedef struct
{
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t virtualAddress;
    uint64_t physicalAddress;
    uint64_t fileSize;
    uint64_t memorySize;
    uint64_t align;
} ElfProgramHeader;

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
} ElfSectionHeader;
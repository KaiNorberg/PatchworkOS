#pragma once

#include "kernel/virtual_memory/virtual_memory.h"
#include "kernel/kernel/kernel.h"

#define ET_NONE 0x00
#define ET_REL 0x01
#define ET_EXEC 0x02
#define ET_DYN 0x03
#define ET_CORE 0x04

#define EM_X86_64 0x3E

#define PT_LOAD 0x00000001

typedef struct
{
    uint8_t Ident[16];
    uint16_t Type;
    uint16_t Machine;
    uint32_t Version;
    uint64_t Entry;
    uint64_t ProgramHeaderOffset;
    uint64_t SectionHeaderOffset;
    uint32_t Flags;
    uint16_t HeaderSize;
    uint16_t ProgramHeaderSize;
    uint16_t ProgramHeaderAmount;
    uint16_t SectionHeaderSize;
    uint16_t SectionHeaderAmount;
    uint16_t SectionHeaderStringIndex;
} ElfHeader;

typedef struct
{
    uint32_t Type;
    uint32_t Flags1;
    uint64_t Offset;
    uint64_t VirtualAddress;
    uint64_t PhysicalAddress;
    uint64_t FileSize;
    uint64_t MemorySize;
    uint32_t Flags2;
    uint64_t Align;
} ElfProgramHeader;

typedef struct
{
    uint32_t Name;
    uint32_t Type;
    uint64_t Flags;
    uint64_t Address;
    uint64_t Offset;
    uint64_t Size;
    uint32_t Link;
    uint32_t Info;
    uint64_t AddressAlign;
    uint64_t EntrySize;
} ElfSectionHeader;

typedef struct
{
    void* Segment;
    uint64_t PageAmount;
} ProgramSegment;

typedef struct
{
    ElfHeader Header;
    ProgramSegment* Segments;
    uint64_t SegmentAmount;
    void* StackBottom;
    uint64_t StackSize;
    VirtualAddressSpace* AddressSpace;
} Program;

Program* load_program(const char* path, BootInfo* bootInfo);

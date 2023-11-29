#pragma once

#include "kernel/gop/gop.h"

typedef struct
{
    uint8_t Present : 1;
    uint8_t ReadWrite : 1;
    uint8_t UserSuper : 1;
    uint8_t WriteThrough : 1;
    uint8_t CacheDisabled : 1;
    uint8_t Accessed : 1;
    uint8_t Ignore0 : 1; 
    uint8_t LargerPages : 1;
    uint8_t Ignore1 : 1;

    uint8_t Available : 3;

    uint64_t Address : 52;
} PageDirEntry;

typedef struct __attribute__((aligned(0x1000)))
{ 
    PageDirEntry Entries[512];
} PageTable;

typedef PageTable VirtualAddressSpace;

VirtualAddressSpace* virtual_memory_create();

void virtual_memory_load_space(VirtualAddressSpace* addressSpace);

void virtual_memory_remap(VirtualAddressSpace* addressSpace, void* virtualAddress, void* physicalAddress);
#pragma once

#include <stdint.h>

typedef struct __attribute__((packed))
{
    uint16_t Size;
    uint64_t Offset;
} GDTDesc;

typedef struct __attribute__((packed))
{
    uint16_t Limit0;
    uint16_t Base0;
    uint8_t Base1;
    uint8_t AccessByte;
    uint8_t Limit1Flags;
    uint8_t Base2;
} GDTEntry;

typedef struct __attribute__((packed)) __attribute__((aligned(0x1000))) 
{
    GDTEntry Null;
    GDTEntry KernelCode;
    GDTEntry KernelData;
    GDTEntry UserNull;
    GDTEntry UserCode;
    GDTEntry UserData;
} GDT;

extern void gdt_load(GDTDesc* GDTDescriptor);
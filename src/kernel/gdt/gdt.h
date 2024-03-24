#pragma once

#include "tss/tss.h"
#include "types/types.h"

#define GDT_NULL 0x00
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE 0x18
#define GDT_USER_DATA 0x20
#define GDT_TSS 0x28

typedef struct __attribute__((packed))
{
    uint16_t size;
    uint64_t offset;
} GdtDesc;  

typedef struct __attribute__((packed))
{
    uint16_t limitLow;
    uint16_t baseLow;
    uint8_t baseMiddle;
    uint8_t access;
    uint8_t flagsAndLimitHigh;
    uint8_t baseHigh;
} SegmentDescriptor;

typedef struct __attribute__((packed))
{
    uint16_t limitLow;
    uint16_t baseLow;
    uint8_t baseLowerMiddle;
    uint8_t access;
    uint8_t flagsAndLimitHigh;
    uint8_t baseUpperMiddle;
    uint32_t baseHigh;
    uint32_t reserved;
} LongSegmentDescriptor;

typedef struct __attribute__((packed))
{
    SegmentDescriptor null;
    SegmentDescriptor kernelCode;
    SegmentDescriptor kernelData;
    SegmentDescriptor userCode;
    SegmentDescriptor userData;
    LongSegmentDescriptor tss;
} Gdt;

extern void gdt_load_descriptor(GdtDesc* descriptor);

void gdt_init(void);

void gdt_load(void);

void gdt_load_tss(Tss* tss);
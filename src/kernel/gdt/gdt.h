#pragma once

#include <stdint.h>

#include "tss/tss.h"

typedef struct __attribute__((packed))
{
    uint16_t size;
    uint64_t offset;
} GDTDesc;

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
} GDT;

extern GDT gdt;

extern void gdt_load(GDTDesc* descriptor);

void gdt_init();
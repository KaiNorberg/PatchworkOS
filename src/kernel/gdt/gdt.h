#pragma once

#include <stdint.h>

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
    uint32_t reserved1;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved2;
    uint64_t ist[7];
    uint64_t reserved3;
    uint16_t reserved4;
    uint16_t iopb;
} TaskStateSegment;

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

extern TaskStateSegment tss;

extern void gdt_load(GDTDesc* descriptor);
extern void tss_load();

void gdt_init(void* rsp0, void* rsp1, void* rsp2);
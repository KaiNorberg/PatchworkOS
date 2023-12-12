#pragma once

#include <stdint.h>

typedef struct __attribute__((packed))
{
    uint16_t Size;
    uint64_t Offset;
} GDTDesc;

typedef struct __attribute__((packed))
{
    uint16_t LimitLow;
    uint16_t BaseLow;
    uint8_t BaseMiddle;
    uint8_t Access;
    uint8_t FlagsAndLimitHigh;
    uint8_t BaseHigh;
} SegmentDescriptor;

typedef struct __attribute__((packed))
{
    uint16_t LimitLow;
    uint16_t BaseLow;
    uint8_t BaseLowerMiddle;
    uint8_t Access;
    uint8_t FlagsAndLimitHigh;
    uint8_t BaseUpperMiddle;
    uint32_t BaseHigh;
    uint32_t Reserved;
} LongSegmentDescriptor;

typedef struct __attribute__((packed))
{
    uint32_t Reserved1;
    uint64_t RSP0;
    uint64_t RSP1;
    uint64_t RSP2;
    uint64_t Reserved2;
    uint64_t IST[7];
    uint64_t Reserved3;
    uint16_t Reserved4;
    uint16_t IOPB;
} TaskStateSegment;

typedef struct __attribute__((packed))
{
    SegmentDescriptor Null;
    SegmentDescriptor KernelCode;
    SegmentDescriptor KernelData;
    SegmentDescriptor UserCode;
    SegmentDescriptor UserData;
    LongSegmentDescriptor TSS;
} GDT;

extern GDT gdt;

extern TaskStateSegment tss;

extern void gdt_load(GDTDesc* descriptor);
extern void tss_load();

void gdt_init(void* RSP0, void* RSP1, void* RSP2);
#pragma once

#include "defs.h"
#include "tss.h"

#define GDT_NULL 0x00
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE 0x18
#define GDT_USER_DATA 0x20
#define GDT_TSS 0x28

#define GDT_RING3 3
#define GDT_RING2 2
#define GDT_RING1 1
#define GDT_RING0 0

typedef struct PACKED
{
    uint16_t size;
    uint64_t offset;
} gdt_desc_t;

typedef struct PACKED
{
    uint16_t limitLow;
    uint16_t baseLow;
    uint8_t baseMiddle;
    uint8_t access;
    uint8_t flagsAndLimitHigh;
    uint8_t baseHigh;
} gdt_entry_t;

typedef struct PACKED
{
    uint16_t limitLow;
    uint16_t baseLow;
    uint8_t baseLowerMiddle;
    uint8_t access;
    uint8_t flagsAndLimitHigh;
    uint8_t baseUpperMiddle;
    uint32_t baseHigh;
    uint32_t reserved;
} gdt_long_entry_t;

typedef struct PACKED
{
    gdt_entry_t null;
    gdt_entry_t kernelCode;
    gdt_entry_t kernelData;
    gdt_entry_t userCode;
    gdt_entry_t userData;
    gdt_long_entry_t tss;
} gdt_t;

extern void gdt_load_descriptor(gdt_desc_t* descriptor);

void gdt_init(void);

void gdt_load(void);

void gdt_load_tss(tss_t* tss);

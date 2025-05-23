#include "gdt.h"

#include "mem/pmm.h"
#include "tss.h"

ALIGNED(PAGE_SIZE) static gdt_t gdt;

static gdt_entry_t gdt_entry_create(uint8_t access, uint8_t flags)
{
    gdt_entry_t entry;
    entry.limitLow = 0;
    entry.baseLow = 0;
    entry.baseMiddle = 0;
    entry.access = access;
    entry.flagsAndLimitHigh = (flags << 4);
    entry.baseHigh = 0;

    return entry;
}

void gdt_init(void)
{
    gdt.null = gdt_entry_create(0, 0);
    gdt.kernelCode = gdt_entry_create(0x9A, 0xA);
    gdt.kernelData = gdt_entry_create(0x92, 0xC);
    gdt.userData = gdt_entry_create(0xF2, 0xC);
    gdt.userCode = gdt_entry_create(0xFA, 0xA);

    gdt_load();
}

void gdt_load(void)
{
    gdt_desc_t gdtDesc;
    gdtDesc.size = sizeof(gdt_t) - 1;
    gdtDesc.offset = (uint64_t)&gdt;
    gdt_load_descriptor(&gdtDesc);
}

void gdt_load_tss(tss_t* tss)
{
    gdt.tssDesc.limitLow = sizeof(tss_t);
    gdt.tssDesc.baseLow = (uint16_t)((uint64_t)tss);
    gdt.tssDesc.baseLowerMiddle = (uint8_t)((uint64_t)tss >> 16);
    gdt.tssDesc.access = 0x89;
    gdt.tssDesc.flagsAndLimitHigh = 0x00; // Flags = 0x0, LimitHigh = 0x0
    gdt.tssDesc.baseUpperMiddle = (uint8_t)((uint64_t)tss >> (16 + 8));
    gdt.tssDesc.baseHigh = (uint32_t)((uint64_t)tss >> (16 + 8 + 8));
    tss_load();
}

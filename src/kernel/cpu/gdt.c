#include <kernel/cpu/gdt.h>

#include <kernel/cpu/tss.h>
#include <kernel/mem/pmm.h>

static gdt_t gdt ALIGNED(PAGE_SIZE);

static gdt_segment_t gdt_segment(uint16_t access, uint16_t flags)
{
    gdt_segment_t entry;
    entry.limitLow = 0;
    entry.baseLow = 0;
    entry.baseMiddle = 0;
    entry.access = access;
    entry.flagsAndLimitHigh = (flags << 4);
    entry.baseHigh = 0;
    return entry;
}

static gdt_long_system_segment_t gdt_long_system_segment(uint16_t access, uint16_t flags, uint64_t base, uint32_t limit)
{
    gdt_long_system_segment_t entry;
    entry.limitLow = (uint16_t)(limit & 0xFFFF);
    entry.baseLow = (uint16_t)(base & 0xFFFF);
    entry.baseLowerMiddle = (uint8_t)((base >> 16) & 0xFF);
    entry.access = access;
    entry.flagsAndLimitHigh = ((flags << 4) & 0xF0) | ((limit >> 16) & 0x0F);
    entry.baseUpperMiddle = (uint8_t)((base >> 24) & 0xFF);
    entry.baseHigh = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    entry.reserved = 0;
    return entry;
}

void gdt_init(void)
{
    gdt.null = gdt_segment(0, 0);
    gdt.kernelCode = gdt_segment(GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_DATA_CODE | GDT_ACCESS_EXEC |
            GDT_ACCESS_READ_WRITE | GDT_ACCESS_ACCESSED,
        GDT_FLAGS_LONG_MODE | GDT_FLAGS_4KB);
    gdt.kernelData = gdt_segment(GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_DATA_CODE | GDT_ACCESS_READ_WRITE |
            GDT_ACCESS_ACCESSED,
        GDT_FLAGS_4KB);
    gdt.userData = gdt_segment(GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_DATA_CODE | GDT_ACCESS_READ_WRITE |
            GDT_ACCESS_ACCESSED,
        GDT_FLAGS_4KB);
    gdt.userCode = gdt_segment(GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_DATA_CODE | GDT_ACCESS_EXEC |
            GDT_ACCESS_READ_WRITE | GDT_ACCESS_ACCESSED,
        GDT_FLAGS_LONG_MODE | GDT_FLAGS_4KB);
    gdt.tssDesc = gdt_long_system_segment(GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_SYSTEM |
            GDT_ACCESS_TYPE_TSS_AVAILABLE | GDT_ACCESS_ACCESSED,
        GDT_FLAGS_NONE, 0, 0);
}

void gdt_cpu_load(void)
{
    gdt_desc_t gdtDesc;
    gdtDesc.size = sizeof(gdt_t) - 1;
    gdtDesc.offset = (uint64_t)&gdt;
    gdt_load_descriptor(&gdtDesc);
}

void gdt_cpu_tss_load(tss_t* tss)
{
    gdt.tssDesc = gdt_long_system_segment(GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_SYSTEM |
            GDT_ACCESS_TYPE_TSS_AVAILABLE | GDT_ACCESS_ACCESSED,
        GDT_FLAGS_NONE, (uint64_t)tss, sizeof(tss_t) - 1);
    tss_load();
}

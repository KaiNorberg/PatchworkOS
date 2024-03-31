#include "gdt.h"

#include "tty/tty.h"
#include "tss/tss.h"

__attribute__((aligned(0x1000)))
static Gdt gdt;

static inline GdtEntry gdt_entry_create(uint8_t access, uint8_t flags)
{
    GdtEntry entry;
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
    gdt.userCode = gdt_entry_create(0xFA, 0xA);
    gdt.userData = gdt_entry_create(0xF2, 0xC);

    gdt_load();
}

void gdt_load(void)
{
    GdtDesc gdtDesc;
	gdtDesc.size = sizeof(Gdt) - 1;
	gdtDesc.offset = (uint64_t)&gdt;
	gdt_load_descriptor(&gdtDesc);  
}

void gdt_load_tss(Tss* tss)
{
    gdt.tss.limitLow = sizeof(Tss);
    gdt.tss.baseLow = (uint16_t)((uint64_t)tss);
    gdt.tss.baseLowerMiddle = (uint8_t)((uint64_t)tss >> 16);
    gdt.tss.access = 0x89;
    gdt.tss.flagsAndLimitHigh = 0x00; //Flags = 0x0, LimitHigh = 0x0
    gdt.tss.baseUpperMiddle = (uint8_t)((uint64_t)tss >> (16 + 8));
    gdt.tss.baseHigh = (uint32_t)((uint64_t)tss >> (16 + 8 + 8));
    tss_load();
}

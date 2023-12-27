#include "gdt.h"

#include "tty/tty.h"
#include "tss/tss.h"
#include "string/string.h"
#include "interrupt_stack/interrupt_stack.h"

__attribute__((aligned(0x1000)))
GDT gdt;

void gdt_init()
{
    tty_start_message("GDT loading");

    memset(&gdt, 0, sizeof(GDT));

    gdt.null.limitLow = 0;
    gdt.null.baseLow = 0;
    gdt.null.baseMiddle = 0;
    gdt.null.access = 0x00;
    gdt.null.flagsAndLimitHigh = 0; //Flags = 0x0, LimitHigh = 0
    gdt.null.baseHigh = 0;

    gdt.kernelCode.limitLow = 0;
    gdt.kernelCode.baseLow = 0;
    gdt.kernelCode.baseMiddle = 0;
    gdt.kernelCode.access = 0x9A;
    gdt.kernelCode.flagsAndLimitHigh = 0xAF; //Flags = 0xA, LimitHigh = 0x0
    gdt.kernelCode.baseHigh = 0;

    gdt.kernelData.limitLow = 0;
    gdt.kernelData.baseLow = 0;
    gdt.kernelData.baseMiddle = 0;
    gdt.kernelData.access = 0x92;
    gdt.kernelData.flagsAndLimitHigh = 0xC0; //Flags = 0xC, LimitHigh = 0x0
    gdt.kernelData.baseHigh = 0;

    gdt.userCode.limitLow = 0;
    gdt.userCode.baseLow = 0;
    gdt.userCode.baseMiddle = 0;
    gdt.userCode.access = 0xFA;
    gdt.userCode.flagsAndLimitHigh = 0xA0; //Flags = 0xA, LimitHigh = 0x0
    gdt.userCode.baseHigh = 0;

    gdt.userData.limitLow = 0;
    gdt.userData.baseLow = 0;
    gdt.userData.baseMiddle = 0;
    gdt.userData.access = 0xF2;
    gdt.userData.flagsAndLimitHigh = 0xC0; //Flags = 0xC, LimitHigh = 0x0
    gdt.userData.baseHigh = 0;

    gdt.tss.limitLow = sizeof(TaskStateSegment);
    gdt.tss.baseLow = (uint16_t)((uint64_t)&tss);
    gdt.tss.baseLowerMiddle = (uint8_t)((uint64_t)&tss >> 16);
    gdt.tss.access = 0x89;
    gdt.tss.flagsAndLimitHigh = 0x00; //Flags = 0x0, LimitHigh = 0x0
    gdt.tss.baseUpperMiddle = (uint8_t)((uint64_t)&tss >> (16 + 8));
    gdt.tss.baseHigh = (uint32_t)((uint64_t)&tss >> (16 + 8 + 8));

    static GDTDesc gdtDesc;
	gdtDesc.size = sizeof(gdt) - 1;
	gdtDesc.offset = (uint64_t)&gdt;
	gdt_load(&gdtDesc);

    tty_end_message(TTY_MESSAGE_OK);
}

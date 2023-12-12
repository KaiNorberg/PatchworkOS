#include "gdt.h"

#include "tty/tty.h"
#include "string/string.h"

__attribute__((aligned(0x1000)))
GDT gdt;

__attribute__((aligned(0x1000)))
TaskStateSegment tss;

void gdt_init(void* RSP0, void* RSP1, void* RSP2)
{
    tty_start_message("GDT loading");

    gdt.Null.LimitLow = 0;
    gdt.Null.BaseLow = 0;
    gdt.Null.BaseMiddle = 0;
    gdt.Null.Access = 0x00;
    gdt.Null.FlagsAndLimitHigh = 0; //Flags = 0x0, LimitHigh = 0
    gdt.Null.BaseHigh = 0;

    gdt.KernelCode.LimitLow = 0;
    gdt.KernelCode.BaseLow = 0;
    gdt.KernelCode.BaseMiddle = 0;
    gdt.KernelCode.Access = 0x9A;
    gdt.KernelCode.FlagsAndLimitHigh = 0xAF; //Flags = 0xA, LimitHigh = 0x0
    gdt.KernelCode.BaseHigh = 0;

    gdt.KernelData.LimitLow = 0;
    gdt.KernelData.BaseLow = 0;
    gdt.KernelData.BaseMiddle = 0;
    gdt.KernelData.Access = 0x92;
    gdt.KernelData.FlagsAndLimitHigh = 0xC0; //Flags = 0xC, LimitHigh = 0x0
    gdt.KernelData.BaseHigh = 0;

    gdt.UserCode.LimitLow = 0;
    gdt.UserCode.BaseLow = 0;
    gdt.UserCode.BaseMiddle = 0;
    gdt.UserCode.Access = 0xFA;
    gdt.UserCode.FlagsAndLimitHigh = 0xA0; //Flags = 0xA, LimitHigh = 0x0
    gdt.UserCode.BaseHigh = 0;

    gdt.UserData.LimitLow = 0;
    gdt.UserData.BaseLow = 0;
    gdt.UserData.BaseMiddle = 0;
    gdt.UserData.Access = 0xF2;
    gdt.UserData.FlagsAndLimitHigh = 0xC0; //Flags = 0xC, LimitHigh = 0x0
    gdt.UserData.BaseHigh = 0;

    memset(&tss, 0, sizeof(TaskStateSegment));
    tss.RSP0 = (uint64_t)RSP0;
    tss.RSP1 = (uint64_t)RSP1;
    tss.RSP2 = (uint64_t)RSP2;
    tss.IOPB = sizeof(TaskStateSegment);

    gdt.TSS.LimitLow = sizeof(TaskStateSegment);
    gdt.TSS.BaseLow = (uint16_t)((uint64_t)&tss);
    gdt.TSS.BaseLowerMiddle = (uint8_t)((uint64_t)&tss >> 16);
    gdt.TSS.Access = 0x89;
    gdt.TSS.FlagsAndLimitHigh = 0x00; //Flags = 0x0, LimitHigh = 0x0
    gdt.TSS.BaseUpperMiddle = (uint8_t)((uint64_t)&tss >> (16 + 8));
    gdt.TSS.BaseHigh = (uint32_t)((uint64_t)&tss >> (16 + 8 + 8));

    static GDTDesc gdtDesc;
	gdtDesc.Size = sizeof(gdt) - 1;
	gdtDesc.Offset = (uint64_t)&gdt;
	gdt_load(&gdtDesc);

    tss_load();

    tty_end_message(TTY_MESSAGE_OK);
}

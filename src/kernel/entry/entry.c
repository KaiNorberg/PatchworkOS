#include "entry.h"

#include "kernel/gdt/gdt.h"
#include "kernel/tty/tty.h"
#include "kernel/idt/idt.h"
#include "kernel/utils/utils.h"
#include "kernel/file_system/file_system.h"

#include "libc/include/stdio.h"

__attribute__((aligned(0x1000)))
GDT gdt = 
{
    {0, 0, 0, 0x00, 0x00, 0}, //NULL
    {0, 0, 0, 0x9A, 0xA0, 0}, //KernelCode
    {0, 0, 0, 0x92, 0xA0, 0}, //KernelData
    {0, 0, 0, 0x00, 0x00, 0}, //UserNull
    {0, 0, 0, 0x9A, 0xA0, 0}, //UserCode
    {0, 0, 0, 0x92, 0xA0, 0}, //UserData
};

void _start(BootInfo* bootInfo)
{
    static GDTDesc gdtDesc;
	gdtDesc.Size = sizeof(GDT) - 1;
	gdtDesc.Offset = (uint64_t)&gdt;
	gdt_load(&gdtDesc);

    file_system_init(bootInfo->RootDirectory);

    idt_init();

    tty_init(bootInfo->Screenbuffer, bootInfo->PSFFonts[0]);

    tty_print("Test 1");

    FileContent* file = fopen("/fonts/zap-vga16.psf", "r");

    //tty_print("FILE");
    //tty_printi(file);
    
    tty_print("Test 2");

    //char buffer[64];
    //fread(buffer, 1, 64, file);*/

    //fclose(file);

    tty_print("Magic should be 1078");
    uint16_t magic = *((uint16_t*)((void*)file->Data));
    char string[64];
    itoa(magic, string);
    tty_print(string);

    while (1)
    {
        asm("hlt");
    }
}
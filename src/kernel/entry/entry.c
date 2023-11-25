#include "entry.h"

#include "gdt/gdt.h"
#include "tty/tty.h"

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

uint32_t IntPow(uint32_t X, uint32_t E)
{
    uint32_t Y = 1;

    for (uint32_t i = 0; i < E; i++)
    {
        Y *= X;
    }

    return Y;
}

uint32_t GetDigit(uint32_t Number, uint32_t Digit)
{
    uint32_t Y = IntPow(10, Digit);
    uint32_t Z = Number / Y;
    uint32_t X2 = Z / 10;
    return Z - X2 * 10;
}

uint32_t GetDigitAmount(uint32_t Number)
{
    uint32_t Digits = 0;
    while (Number != 0)
    {
        Number /= 10;
        Digits++;
    }

    return Digits;
}

char IntToStringOutput[128];
char* ToString(uint64_t Number)
{
    uint32_t DigitAmount = GetDigitAmount(Number);

    if (DigitAmount == 0)
    {
        IntToStringOutput[0] = '0';
        IntToStringOutput[1] = 0;           
    }
    else
    {
        for (uint32_t i = DigitAmount; i-- > 0;)
        {
            IntToStringOutput[DigitAmount - i - 1] = '0' + GetDigit(Number, i);
        }
        IntToStringOutput[DigitAmount] = '\0';         
    }

    return IntToStringOutput;
}

void _start(BootInfo* bootInfo)
{
    static GDTDesc gdtDesc;
	gdtDesc.Size = sizeof(GDT) - 1;
	gdtDesc.Offset = (uint64_t)&gdt;
	gdt_load(&gdtDesc);

    tty_init(bootInfo->Screenbuffer, bootInfo->PSFFonts[0]);

    tty_print("Hello, World!");

    tty_print(ToString(*((uint16_t*)bootInfo->RootDirectory->Directories[0].Files[0].Data)));

    while (1)
    {
        asm("hlt");
    }
}
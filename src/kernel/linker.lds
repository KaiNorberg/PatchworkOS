OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0xFFFFFFFF80000000;

    _kernelStart = .;

    .text : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata)
    }

    .data : ALIGN(4K)
    {
        *(.data)
    }

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }

    ._tests : 
    {
        _testsStart = .;
        KEEP(*(._tests))
        _testsEnd = .;
    }

    .syscall_table :
    {
        _syscallTableStart = .;
        KEEP(*(.syscall_table))
        _syscallTableEnd = .;
    }

    _kernelEnd = .;
}

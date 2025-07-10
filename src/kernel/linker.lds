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

    .trampoline : ALIGN(4K)
    {
        *(.trampoline)
    }

    .syscall_table : ALIGN(4K)
    {
        _syscallTableStart = .;
        KEEP(*(.syscall_table))
        _syscallsTableEnd = .;
    }

    .tests : ALIGN(4K)
    {
        _testsStart = .;
        KEEP(*(.tests))
        _testsEnd = .;
    }

    _kernelEnd = .;
}

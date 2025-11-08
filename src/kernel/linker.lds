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

    .syscall_table :
    {
        _syscallTableStart = .;
        KEEP(*(.syscall_table))
        _syscallTableEnd = .;
    }

    .acpi_sdt_handlers :
    {
        _acpiSdtHandlersStart = .;
        KEEP(*(.acpi_sdt_handlers*))
        _acpiSdtHandlersEnd = .;
    }

    _kernelEnd = .;
}

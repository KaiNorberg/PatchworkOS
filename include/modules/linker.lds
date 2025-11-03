OUTPUT_FORMAT(elf64-x86-64)
ENTRY(module_procedure)

SECTIONS
{
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

    .module_info : ALIGN(4K)
    {
        KEEP(*(.module_info))
    }

    .module_acpi_hids : ALIGN(4K)
    {
        KEEP(*(.module_acpi_hids))
    }
}

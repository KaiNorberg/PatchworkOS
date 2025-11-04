OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_module_procedure)

SECTIONS
{
    .text : ALIGN(4K)
    {
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

    ._module_info : 
    {
        KEEP(*(._module_info))
    }

    ._module_device_ids :
    {
        KEEP(*(._module_device_ids))
    }
}

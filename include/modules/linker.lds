OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_module_procedure)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    .text : ALIGN(4K)
    {
        *(.text .text.*)
    } :text

    .rodata : ALIGN(4K)
    {
        *(.rodata .rodata.*)
    } :text

    .data : ALIGN(4K)
    {
        *(.data .data.*)
    } :data

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss .bss.*)
    } :data

    ._percpu : ALIGN(4K)
    {
        _percpuStart = .;
        KEEP(*(._percpu))
        _percpuEnd = .;
    } :data

    ._tests : ALIGN(4K)
    {
        _testsStart = .;
        KEEP(*(._tests))
        _testsEnd = .;
    } :data
    
    ._module_info : ALIGN(4K)
    {
        KEEP(*(._module_info))
    } :data
}
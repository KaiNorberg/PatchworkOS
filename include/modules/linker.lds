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
        *(.text)
    } :text

    .rodata : ALIGN(4K)
    {
        *(.rodata)
    } :text

    .data : ALIGN(4K)
    {
        *(.data)
    } :data

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    } :data

    .init_array : 
    {
        _initArrayStart = .;
        KEEP(*(.init_array .init_array.*))
        _initArrayEnd = .;
    } :data

    .fini_array : 
    {
        _finiArrayStart = .;
        KEEP(*(.fini_array .fini_array.*))
        _finiArrayEnd = .;
    } :data

    ._percpu : 
    {
        _percpuStart = .;
        KEEP(*(._percpu))
        _percpuEnd = .;
    } :data

    ._tests : 
    {
        _testsStart = .;
        KEEP(*(._tests))
        _testsEnd = .;
    } :data
    
    ._module_info : 
    {
        KEEP(*(._module_info))
    } :data
}

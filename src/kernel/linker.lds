OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0xFFFFFFFF80000000;

    _kernelStart = .;

    .text : ALIGN(4K)
    {
        *(.multiboot)
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

    ._tests : 
    {
        _testsStart = .;
        KEEP(*(._tests))
        _testsEnd = .;
    } :data

    .syscall_table :
    {
        _syscallTableStart = .;
        KEEP(*(.syscall_table))
        _syscallTableEnd = .;
    } :data

    _kernelEnd = .;
}

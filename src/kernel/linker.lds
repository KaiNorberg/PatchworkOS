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

    ._early_init_stack : ALIGN(4K)
    {
        _earlyInitStackStart = .; 
        . += 4K;
        _earlyInitStackEnd = .;
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

    ._syscall_table : ALIGN(4K)
    {
        _syscallTableStart = .;
        KEEP(*(._syscall_table))
        _syscallTableEnd = .;
    } :data

    _kernelEnd = .;
}

OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_module_procedure)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
    dynamic PT_DYNAMIC FLAGS(6);
}

SECTIONS
{
    .text : ALIGN(4K)
    {
        *(.multiboot)
        *(.text .text.*)
    } :text

    .rodata : ALIGN(4K)
    {
        *(.rodata .rodata.*)
        *(.eh_frame)
        *(.eh_frame_hdr)
    } :text

    .gnu.hash : 
    {
         *(.gnu.hash) 
    } :text
    .dynsym : 
    { 
        *(.dynsym) 
    } :text
    .dynstr : 
    {
        *(.dynstr) 
    } :text
    .rela.dyn : 
    {
        *(.rela.dyn) *(.rela.plt) 
    } :text
    .rela.plt : 
    {
        *(.rela.plt) 
    } :text
    .plt : 
    {
        *(.plt) 
    } :text

    . = ALIGN(4K);

    .dynamic : 
    {
        *(.dynamic) 
    } :data :dynamic

    .data : ALIGN(4K)
    {
        *(.data .data.*)
        *(.data.rel.ro .data.rel.ro.*)
        *(.got .got.*)
        *(.got.plt)
        *(.init_array .init_array.*)
        *(.fini_array .fini_array.*)
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

    /DISCARD/ :
    {
        *(.comment)
        *(.interp)
    }
}
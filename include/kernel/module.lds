OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
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
        *(.text .text.*)
    } :text

    .rodata : ALIGN(4K)
    {
        *(.rodata .rodata.*)
        *(.eh_frame .eh_frame.*)
        *(.eh_frame_hdr)
        *(.note.gnu.build-id)
    } :text

    .gnu.hash : { *(.gnu.hash) } :text
    .dynsym   : { *(.dynsym) } :text
    .dynstr   : { *(.dynstr) } :text
    .rela.dyn : { *(.rela.dyn) } :text
    .rela.plt : { *(.rela.plt) } :text
    .plt : { *(.plt .plt.*) } :text

    . = ALIGN(4K);

    .dynamic : { *(.dynamic) } :data :dynamic

    .got : { *(.got) } :data
    .got.plt : { *(.got.plt) } :data

    .data : ALIGN(4K)
    {
        *(.data .data.*)
        *(.data.rel.ro .data.rel.ro.*)
        *(.init_array .init_array.*)
        *(.fini_array .fini_array.*)
    } :data

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss .bss.*)
    } :data

    . = ALIGN(4K);
    ._percpu :
    {
        _percpu_start = .;
        KEEP(*(._percpu))
        _percpu_end = .;
    } :data

    . = ALIGN(4K);
    ._tests :
    {
        _tests_start = .;
        KEEP(*(._tests))
        _tests_end = .;
    } :data

    . = ALIGN(4K);
    ._module_info :
    {
        KEEP(*(._module_info))
    } :data

    /DISCARD/ :
    {
        *(.note.GNU-stack)
        *(.comment)
        *(.interp)
    }
}
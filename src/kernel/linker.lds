OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

KERNEL_START = 0xFFFFFFFF80000000;

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = KERNEL_START;

    _kernel_start = .;

    .text : AT(ADDR(.text) - KERNEL_START)
    {
        _text = .;
        *(.multiboot)
        *(.text .text.*)
        _etext = .;
    } :text

    . = ALIGN(4K);

    .rodata : AT(ADDR(.rodata) - KERNEL_START)
    {
        _rodata = .;
        *(.rodata .rodata.*)
        *(.eh_frame .eh_frame.*)
        *(.eh_frame_hdr)
        *(.note.gnu.build-id)
        _erodata = .;
    } :text

    . = ALIGN(4K);
    _etext_segment = .;

    .data : AT(ADDR(.data) - KERNEL_START)
    {
        _sdata = .;
        *(.data .data.*)
        *(.data.rel.ro .data.rel.ro.*)

        *(.got .got.*)

        *(.init_array .init_array.*)
        PROVIDE_HIDDEN(__init_array_start = .);
        PROVIDE_HIDDEN(__init_array_end = .);
        *(.fini_array .fini_array.*)
        PROVIDE_HIDDEN(__fini_array_start = .);
        PROVIDE_HIDDEN(__fini_array_end = .);

        _edata = .;
    } :data

    . = ALIGN(4K);

    .bss : AT(ADDR(.bss) - KERNEL_START)
    {
        _sbss = .;
        *(COMMON)
        *(.bss .bss.*)
        . = ALIGN(4K);
        _ebss = .;
    } :data

    . = ALIGN(4K);
    ._early_init_stack : AT(ADDR(._early_init_stack) - KERNEL_START)
    {
        _early_init_stack_start = .;
        . += 4K;
        _early_init_stack_end = .;
    } :data

    . = ALIGN(4K);
    ._percpu : AT(ADDR(._percpu) - KERNEL_START)
    {
        _percpu_start = .;
        KEEP(*(._percpu))
        _percpu_end = .;
    } :data

    . = ALIGN(4K);
    ._tests : AT(ADDR(._tests) - KERNEL_START)
    {
        _tests_start = .;
        KEEP(*(._tests))
        _tests_end = .;
    } :data

    . = ALIGN(4K);
    ._syscall_table : AT(ADDR(._syscall_table) - KERNEL_START)
    {
        _syscall_table_start = .;
        KEEP(*(._syscall_table))
        _syscall_table_end = .;
    } :data

    . = ALIGN(4K);
    ._irp_table : AT(ADDR(._irp_table) - KERNEL_START)
    {
        _irp_table_start = .;
        KEEP(*(._irp_table))
        _irp_table_end = .;
    } :data

    . = ALIGN(4K);
    _kernel_end = .;

    .got.plt (INFO) : { *(.got.plt) }
    .plt (INFO) : { *(.plt) *(.plt.*) *(.iplt) }

    /DISCARD/ :
    {
        *(.note.GNU-stack)
        *(.comment)
        *(.gnu.hash)
        *(.interp)
        *(.rel.*)
        *(.rela.*)
    }
}

ASSERT(_etext_segment > _text, "Text segment is empty or invalid")
ASSERT(_kernel_end > _kernel_start, "Kernel has no content")

.code64

#include "kernel/cpu/gdt.h"

.global gdt_load_descriptor
.type gdt_load_descriptor, @function

.text

// rdi = gdt descriptor
gdt_load_descriptor:
    lgdt (%rdi)
    pushq $GDT_KERNEL_CODE
    leaq .reload_CS(%rip), %rax
    pushq %rax
    lretq
.reload_CS:
    movw $GDT_KERNEL_DATA, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    ret
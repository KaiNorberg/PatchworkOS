.code64
.intel_syntax noprefix

#include "kernel/cpu/gdt.h"

.global gdt_load_descriptor
.type gdt_load_descriptor, @function

.text

// rdi = gdt descriptor
gdt_load_descriptor:
    lgdt  [rdi]
    push GDT_KERNEL_CODE
    lea rax, [rip + .reload_CS]
    push rax
    rex.w jmp fword ptr [rsp]
.reload_CS:
    add rsp, 16
    mov ax, GDT_KERNEL_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret
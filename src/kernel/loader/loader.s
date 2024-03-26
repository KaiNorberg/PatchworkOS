[bits 64]

%include "gdt/gdt.inc"
%include "registers/registers.inc"

section .loader

extern loader_load
extern loader_allocate_stack

global loader_entry
loader_entry:
    call loader_allocate_stack

    push GDT_USER_DATA | 3
    push rax
    push RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET
    push GDT_USER_CODE | 3
    push loader_load
    iretq
    
;Janky but it avoids code duplication
%define .text .loader
%include "internal/syscalls/syscalls.s"
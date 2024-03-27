[bits 64]

%include "gdt/gdt.inc"
%include "registers/registers.inc"

section .text

;rdi = rsp
;rsi = rip
global loader_jump_to_user_space
loader_jump_to_user_space:
    push GDT_USER_DATA | 3
    push rdi
    push RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET
    push GDT_USER_CODE | 3
    push rsi
    iretq
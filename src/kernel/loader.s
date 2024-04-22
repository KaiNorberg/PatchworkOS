[bits 64]

%include "kernel/gdt.inc"
%include "kernel/regs.inc"

section .text

;rdi = rsp
;rsi = rip
global loader_jump_to_user_space
loader_jump_to_user_space:
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rbp, rbp
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15

    push GDT_USER_DATA | 3
    push rdi
    push RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET
    push GDT_USER_CODE | 3
    push rsi
    iretq
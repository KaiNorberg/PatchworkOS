[bits 64]

%include "cpu/gdt.inc"
%include "cpu/regs.inc"

section .text

;rdi = argc
;rsi = argv
;rdx = rsp
;rcx = rip
global loader_jump_to_user_space
loader_jump_to_user_space:
    xor rax, rax
    xor rbx, rbx
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
    push rdx
    push RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET
    push GDT_USER_CODE | 3
    push rcx
    iretq

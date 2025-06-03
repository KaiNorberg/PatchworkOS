[bits 64]

%include "cpu/trap.inc"

section .text

extern kernel_checkpoint
extern smp_self_unsafe

global kernel_checkpoint_invoke
kernel_checkpoint_invoke:
    xor rax, rax ; Set to zero to prevent weirdness with the ss and cs registers.
    mov r8, [rsp] ; Get the return address.
    ; Store the stack pointer from before this function was called
    mov r9, rsp
    add r9, 8

    ; Construct trap frame
    mov ax, ss
    push rax
    push r9
    pushfq
    mov ax, cs
    push rax
    push r8

    ; Ignore vector and error code.
    push qword 0
    push qword 0

    TRAP_FRAME_REGS_PUSH

    cli

    call smp_self_unsafe
    mov rsi, rax

    mov rbp, rsp
    mov rdi, rsp
    call kernel_checkpoint
    cmp rax, 0
    jnz .scheduling_occoured

    sti
    add rsp, 176 ; Pop the entire trap frame and discard
    ret
.scheduling_occoured:
    TRAP_FRAME_REGS_POP
    add rsp, 16
    iretq

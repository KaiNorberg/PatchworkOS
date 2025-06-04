[bits 64]

%include "cpu/trap.inc"

section .text

extern sched_schedule
extern smp_self_unsafe

global sched_idle_loop
sched_idle_loop:
    sti
    hlt
    jmp sched_idle_loop

global sched_invoke
sched_invoke:
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
    call sched_schedule
    cmp rax, 0
    jnz .scheduling_occoured

    sti
    add rsp, 176 ; Pop the entire trap frame and discard
    ret
.scheduling_occoured:
    TRAP_FRAME_REGS_POP
    add rsp, 16
    iretq ; Original rflags are loaded here so the lack of a "sti" operation does not matter.

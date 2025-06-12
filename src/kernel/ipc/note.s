[bits 64]

%include "cpu/trap.inc"

section .text

extern note_dispatch
extern smp_self_unsafe

global note_dispatch_invoke
note_dispatch_invoke:
    TRAP_FRAME_CONSTRUCT

    cli

    call smp_self_unsafe
    mov rsi, rax

    mov rdi, rsp
    call note_dispatch
    cmp rax, 0
    jnz .trap_frame_modified

    sti
    add rsp, 176 ; Pop the entire trap frame and discard
    ret
.trap_frame_modified:
    TRAP_FRAME_REGS_POP
    add rsp, 16
    iretq ; Original rflags are loaded here so the lack of a "sti" operation does not matter.

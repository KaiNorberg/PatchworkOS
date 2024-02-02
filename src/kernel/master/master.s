[bits 64]

extern kernel_stack_top

extern dispatcher_fetch

global master_entry

master_entry:
    mov rsp, kernel_stack_top
    xor rbp, rbp
.loop_start:
    cli
    call dispatcher_fetch
    sti
    test rax, rax
    jz .not_available
    call rax
.not_available:
    hlt
    jmp .loop_start
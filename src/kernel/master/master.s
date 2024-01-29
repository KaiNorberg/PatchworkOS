[bits 64]

extern kernel_stack_top

global master_loop

master_loop:
    mov rsp, kernel_stack_top
    mov rbp, 0
    sti
.l1:
    hlt
    jmp .l1
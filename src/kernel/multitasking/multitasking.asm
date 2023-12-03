section .text

global switch_task

switch_task:

    cli

    ;Instuction pointer is at top of stack
    pushfq
    push qword rax
    push qword rbx
    push qword rcx
    push qword rdx
    push qword rdi
    push qword rsi
    push qword rbp
    push qword r8
    push qword r9
    push qword r10
    push qword r11
    push qword r12
    push qword r13
    push qword r14
    push qword r15

    mov qword [rdi + 0], qword rsp
    mov qword rax, cr3
    mov qword [rdi + 8], qword rax

    mov qword rsp, qword [rsi + 0]
    mov qword rdi, qword [rsi + 8]
    mov cr3, rdi
    
    pop qword r15
    pop qword r14
    pop qword r13
    pop qword r12
    pop qword r11
    pop qword r10
    pop qword r9
    pop qword r8
    pop qword rbp
    pop qword rsi
    pop qword rdi
    pop qword rdx
    pop qword rcx
    pop qword rbx
    pop qword rax
    popfq   
     
    sti

    ret ;Will pop the last item on stack (RIP)
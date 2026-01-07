.code64
.intel_syntax noprefix

.extern main
.extern exit
.extern _std_init

.global _start
.type _start, @function

.text

_start:	
    mov rbp, rsp    

    // Check kernel_sched_loader for the stack layout and register setup

    push rsi
    push rdi

    call _std_init

    pop rdi
    pop rsi

    call main
    push rax

    pop rdi
    call exit
    ud2
.end:
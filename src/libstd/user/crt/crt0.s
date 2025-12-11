extern main
extern exit
extern _init
extern _std_init
extern environ

global _start:function (_start.end - _start)

section .text
_start:	
    mov rbp, rsp    

    ; Check kernel_sched_loader for the stack layout and register setup

	push rsi
	push rdi

    call _init

    call _std_init

    pop rdi
    pop rsi

    call main
    push rax

    ;call _StdDeinit

    pop rdi
    call exit
    ud2
.end:
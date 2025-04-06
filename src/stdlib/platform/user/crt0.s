extern main
extern exit
extern _init
extern _StdInit

section .text
global _start:function (_start.end - _start)
_start:	
    mov rbp, 0
	push rbp
	push rbp
	mov rbp, rsp

	push rsi
	push rdi

    call _init

    call _StdInit

    pop rdi
    pop rsi

    call main
    push rax

    ;call _StdDeinit

    pop rdi
    call exit
    ud2
.end:
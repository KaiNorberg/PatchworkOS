[bits 64]

%include "cpu/interrupt.inc"

%macro VECTOR_NAME 1
    dq vector_%1
%endmacro

%macro VECTOR_ERR 1
vector_%1:
    push qword %1
    jmp vector_common
%endmacro

%macro VECTOR_NO_ERR 1
vector_%1:
    push qword 0
    push qword %1
    jmp vector_common
%endmacro

extern interrupt_handler

section .text

vector_common:
    INTERRUPT_FRAME_REGS_PUSH

    mov rbp, rsp
    mov rdi, rsp
    call interrupt_handler

    INTERRUPT_FRAME_POP_AND_JUMP

VECTOR_NO_ERR 0
VECTOR_NO_ERR 1
VECTOR_NO_ERR 2
VECTOR_NO_ERR 3
VECTOR_NO_ERR 4
VECTOR_NO_ERR 5
VECTOR_NO_ERR 6
VECTOR_NO_ERR 7
VECTOR_ERR 8
VECTOR_NO_ERR 9
VECTOR_ERR 10
VECTOR_ERR 11
VECTOR_ERR 12
VECTOR_ERR 13
VECTOR_ERR 14
VECTOR_NO_ERR 15
VECTOR_NO_ERR 16
VECTOR_ERR 17
VECTOR_NO_ERR 18
VECTOR_NO_ERR 19
VECTOR_NO_ERR 20
VECTOR_NO_ERR 21
VECTOR_NO_ERR 22
VECTOR_NO_ERR 23
VECTOR_NO_ERR 24
VECTOR_NO_ERR 25
VECTOR_NO_ERR 26
VECTOR_NO_ERR 27
VECTOR_NO_ERR 28
VECTOR_NO_ERR 29
VECTOR_ERR 30
VECTOR_NO_ERR 31
%assign i 32
%rep 224
    VECTOR_NO_ERR i
%assign i i+1
%endrep

section .data

global vectorTable
vectorTable:
%assign i 0
%rep 256
    VECTOR_NAME i
%assign i i+1
%endrep

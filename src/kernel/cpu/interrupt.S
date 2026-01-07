.code64
.intel_syntax noprefix

#include "kernel/cpu/interrupt.h"

.macro INTERRUPT_NAME nr
    .quad vector_\nr
.endm

.macro INTERRUPT_ERR nr
vector_\nr:
    push \nr
    jmp vector_common
.endm

.macro INTERRUPT_NO_ERR nr
vector_\nr:
    push 0
    push \nr
    jmp vector_common
.endm

.extern interrupt_handler
.extern cpu_interrupt_stack_top
.extern memcpy
.global interrupt_fake
.type interrupt_fake, @function

.text

vector_common:
    INTERRUPT_FRAME_REGS_PUSH

    mov rbp, rsp
    mov rdi, rsp
    call interrupt_handler

    INTERRUPT_FRAME_REGS_POP

    add rsp, 16 // Pop error code and vector number
    iretq

INTERRUPT_NO_ERR 0
INTERRUPT_NO_ERR 1
INTERRUPT_NO_ERR 2
INTERRUPT_NO_ERR 3
INTERRUPT_NO_ERR 4
INTERRUPT_NO_ERR 5
INTERRUPT_NO_ERR 6
INTERRUPT_NO_ERR 7
INTERRUPT_ERR 8
INTERRUPT_NO_ERR 9
INTERRUPT_ERR 10
INTERRUPT_ERR 11
INTERRUPT_ERR 12
INTERRUPT_ERR 13
INTERRUPT_ERR 14
INTERRUPT_NO_ERR 15
INTERRUPT_NO_ERR 16
INTERRUPT_ERR 17
INTERRUPT_NO_ERR 18
INTERRUPT_NO_ERR 19
INTERRUPT_NO_ERR 20
INTERRUPT_NO_ERR 21
INTERRUPT_NO_ERR 22
INTERRUPT_NO_ERR 23
INTERRUPT_NO_ERR 24
INTERRUPT_NO_ERR 25
INTERRUPT_NO_ERR 26
INTERRUPT_NO_ERR 27
INTERRUPT_NO_ERR 28
INTERRUPT_NO_ERR 29
INTERRUPT_ERR 30
INTERRUPT_NO_ERR 31

.altmacro
.set i, 32
.rept 224
    INTERRUPT_NO_ERR %i
    .set i, i+1
.endr

// rdi = interrupt_frame_t* frame
// rsi = cpu_t* self
interrupt_fake:
    push 0 // align stack

    mov r15, rdi // frame
    
    // memcpy((r14 = cpu_interrupt_stack_top(self) - INTERRUPT_FRAME_SIZE), frame, INTERRUPT_FRAME_SIZE)
    mov rdi, rsi
    call cpu_interrupt_stack_top
    mov rdi, rax
    sub rdi, INTERRUPT_FRAME_SIZE
    mov r14, rdi
    mov rsi, r15
    mov rdx, INTERRUPT_FRAME_SIZE
    call memcpy

    // interrupt_handler((rbp = rsp = r14))
    mov rsp, r14
    mov rbp, rsp
    mov rdi, rsp
    call interrupt_handler

    INTERRUPT_FRAME_REGS_POP
    add rsp, 16
    iretq

.data

.global vectorTable
.type vectorTable, @object
vectorTable:
.set i, 0
.rept 256
    INTERRUPT_NAME %i
    .set i, i+1
.endr
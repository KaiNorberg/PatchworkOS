.code64

#include "kernel/cpu/interrupt.h"

.macro INTERRUPT_NAME nr
    .quad vector_\nr
.endm

.macro INTERRUPT_ERR nr
vector_\nr:
    pushq $\nr
    jmp vector_common
.endm

.macro INTERRUPT_NO_ERR nr
vector_\nr:
    pushq $0
    pushq $\nr
    jmp vector_common
.endm

.extern interrupt_handler
.extern cpu_interrupt_stack_top
.extern memcpy
.global interrupt_fake
.type interrupt_fake, @function

.text

vector_common:
    // Swap to kernel GS if we are from user space
    testb $3, 24(%rsp)
    jz 1f
    swapgs
1:
    INTERRUPT_FRAME_REGS_PUSH

    movq %rsp, %rbp
    movq %rsp, %rdi
    call interrupt_handler

    INTERRUPT_FRAME_REGS_POP

    testb $3, 24(%rsp)
    jz 1f
    swapgs
1:
    addq $16, %rsp // Pop error code and vector number
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
interrupt_fake:
    pushq $0 // align stack

    movq %rdi, %r15 // frame
    
    // memcpy((r14 = cpu_interrupt_stack_top() - INTERRUPT_FRAME_SIZE), frame, INTERRUPT_FRAME_SIZE)
    call cpu_interrupt_stack_top
    movq %rax, %rdi
    subq $INTERRUPT_FRAME_SIZE, %rdi
    movq %rdi, %r14
    movq %r15, %rsi
    movq $INTERRUPT_FRAME_SIZE, %rdx
    call memcpy

    // interrupt_handler((rbp = rsp = r14))
    movq %r14, %rsp
    movq %rsp, %rbp
    movq %rsp, %rdi
    call interrupt_handler

    INTERRUPT_FRAME_REGS_POP
    addq $16, %rsp
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
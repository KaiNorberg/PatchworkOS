.code64

.global longjmp
.type longjmp, @function

.text

// rdi = address of jmp_buf
// rsi = return value
longjmp:
    testq %rsi, %rsi
    jnz .valid_val
    movq $1, %rsi
    
.valid_val:
    movl 64(%rdi), %eax
    subq $8, %rsp
    movl %eax, (%rsp)
    ldmxcsr (%rsp)
    
    movl 68(%rdi), %eax
    movw %ax, (%rsp)
    fldcw (%rsp)
    addq $8, %rsp
    
    movq (%rdi), %rbx
    movq 8(%rdi), %rbp
    movq 16(%rdi), %r12
    movq 24(%rdi), %r13
    movq 32(%rdi), %r14
    movq 40(%rdi), %r15
    
    movq 56(%rdi), %rdx
    
    movq 48(%rdi), %rsp
    
    movq %rsi, %rax
    
    jmp *%rdx
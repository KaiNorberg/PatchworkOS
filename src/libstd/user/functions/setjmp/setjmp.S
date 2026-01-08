.code64

.global setjmp
.type setjmp, @function

.text

// rdi = address of jmp_buf
setjmp:
    movq %rbx, (%rdi)
    movq %rbp, 8(%rdi)
    movq %r12, 16(%rdi)
    movq %r13, 24(%rdi)
    movq %r14, 32(%rdi)
    movq %r15, 40(%rdi)
    
    leaq 8(%rsp), %rax
    movq %rax, 48(%rdi)
    
    movq (%rsp), %rax
    movq %rax, 56(%rdi)

    subq $8, %rsp
    stmxcsr (%rsp)
    movl (%rsp), %eax
    movl %eax, 64(%rdi)
    
    fnstcw (%rsp)
    movzwl (%rsp), %eax
    movl %eax, 68(%rdi)
    addq $8, %rsp
    
    xorl %eax, %eax
    ret
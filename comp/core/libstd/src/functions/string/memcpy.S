.code64

.text

#ifndef _KERNEL_

.global memcpy_sse2
.type memcpy_sse2, @function

// rdi = dest
// rsi = src
// rdx = n
// return rdi
memcpy_sse2:
    movq %rdi, %rax
    testq %rdx, %rdx
    jz .done

.align_loop:
    testq %rdx, %rdx
    jz .done
    testq $15, %rdi
    jz .aligned
    movb (%rsi), %cl
    movb %cl, (%rdi)
    incq %rdi
    incq %rsi
    decq %rdx
    jnz .align_loop
    
.aligned:
    cmpq $262144, %rdx // 256KB
    jb .small_copy

.large_copy: // Non temporal stores
    cmpq $256, %rdx
    jb .large_copy_done
    prefetchnta 512(%rsi)
    movdqu (%rsi), %xmm0
    movdqu 16(%rsi), %xmm1
    movdqu 32(%rsi), %xmm2
    movdqu 48(%rsi), %xmm3
    movdqu 64(%rsi), %xmm4
    movdqu 80(%rsi), %xmm5
    movdqu 96(%rsi), %xmm6
    movdqu 112(%rsi), %xmm7
    movdqu 128(%rsi), %xmm8
    movdqu 144(%rsi), %xmm9
    movdqu 160(%rsi), %xmm10
    movdqu 176(%rsi), %xmm11
    movdqu 192(%rsi), %xmm12
    movdqu 208(%rsi), %xmm13
    movdqu 224(%rsi), %xmm14
    movdqu 240(%rsi), %xmm15
    movntdq %xmm0, (%rdi)
    movntdq %xmm1, 16(%rdi)
    movntdq %xmm2, 32(%rdi)
    movntdq %xmm3, 48(%rdi)
    movntdq %xmm4, 64(%rdi)
    movntdq %xmm5, 80(%rdi)
    movntdq %xmm6, 96(%rdi)
    movntdq %xmm7, 112(%rdi)
    movntdq %xmm8, 128(%rdi)
    movntdq %xmm9, 144(%rdi)
    movntdq %xmm10, 160(%rdi)
    movntdq %xmm11, 176(%rdi)
    movntdq %xmm12, 192(%rdi)
    movntdq %xmm13, 208(%rdi)
    movntdq %xmm14, 224(%rdi)
    movntdq %xmm15, 240(%rdi)
    addq $256, %rdi
    addq $256, %rsi
    subq $256, %rdx
    jmp .large_copy

.large_copy_done:
    sfence

.small_copy:
    cmpq $64, %rdx
    jb .copy_tail
    prefetchnta 256(%rsi)
    movdqu (%rsi), %xmm0
    movdqu 16(%rsi), %xmm1
    movdqu 32(%rsi), %xmm2
    movdqu 48(%rsi), %xmm3
    movdqa %xmm0, (%rdi)
    movdqa %xmm1, 16(%rdi)
    movdqa %xmm2, 32(%rdi)
    movdqa %xmm3, 48(%rdi)
    addq $64, %rdi
    addq $64, %rsi
    subq $64, %rdx
    jmp .small_copy

.copy_tail:
    testq %rdx, %rdx
    jz .done
    movb (%rsi), %cl
    movb %cl, (%rdi)
    incq %rdi
    incq %rsi
    decq %rdx
    jmp .copy_tail

.done:
    ret

#endif // _KERNEL_
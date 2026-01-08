.code64
.intel_syntax noprefix

.global setjmp
.type setjmp, @function

.text

// rdi = address of jmp_buf
setjmp:
    mov [rdi + 0], rbx    
    mov [rdi + 8], rbp      
    mov [rdi + 16], r12     
    mov [rdi + 24], r13     
    mov [rdi + 32], r14 
    mov [rdi + 40], r15   
    
    lea rax, [rsp + 8]
    mov [rdi + 48], rax
    
    mov rax, [rsp]
    mov [rdi + 56], rax

    sub rsp, 8
    stmxcsr dword ptr [rsp]
    mov eax, dword ptr [rsp]
    mov [rdi + 64], eax
    
    fnstcw word ptr [rsp] 
    movzx eax, word ptr [rsp]
    mov [rdi + 68], eax
    add rsp, 8
    
    xor eax, eax
    ret
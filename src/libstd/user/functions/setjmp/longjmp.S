.code64
.intel_syntax noprefix

.global longjmp
.type longjmp, @function

.text

// rdi = address of jmp_buf
// rsi = return value
longjmp:
    test rsi, rsi
    jnz .valid_val
    mov rsi, 1
    
.valid_val:
    mov eax, dword ptr [rdi + 64]
    sub rsp, 8  
    mov dword ptr [rsp], eax   
    ldmxcsr dword ptr [rsp]    
    
    mov eax, dword ptr [rdi + 68]  
    mov word ptr [rsp], ax 
    fldcw word ptr [rsp]          
    add rsp, 8 
    
    mov rbx, qword ptr [rdi + 0]    
    mov rbp, qword ptr [rdi + 8] 
    mov r12, qword ptr [rdi + 16]    
    mov r13, qword ptr [rdi + 24]    
    mov r14, qword ptr [rdi + 32]    
    mov r15, qword ptr [rdi + 40]    
    
    mov rdx, qword ptr [rdi + 56]
    
    mov rsp, qword ptr [rdi + 48]
    
    mov rax, rsi
    
    jmp rdx
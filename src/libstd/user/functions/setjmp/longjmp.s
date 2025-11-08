[bits 64]

global longjmp:function

section .text

; rdi = address of jmp_buf
; rsi = return value
longjmp:
    test rsi, rsi
    jnz .valid_val
    mov rsi, 1
    
.valid_val:
    mov eax, [rdi + 64]
    sub rsp, 8  
    mov [rsp], eax   
    ldmxcsr [rsp]    
    
    mov eax, [rdi + 68]  
    mov [rsp], ax 
    fldcw [rsp]          
    add rsp, 8 
    
    mov rbx, [rdi + 0]    
    mov rbp, [rdi + 8] 
    mov r12, [rdi + 16]    
    mov r13, [rdi + 24]    
    mov r14, [rdi + 32]    
    mov r15, [rdi + 40]    
    
    mov rdx, [rdi + 56]
    
    mov rsp, [rdi + 48]
    
    mov rax, rsi
    
    jmp rdx
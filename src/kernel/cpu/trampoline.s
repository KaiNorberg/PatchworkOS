%include "cpu/interrupt.inc"

extern sched_thread
extern thread_load
extern init_other_cpu

%define TRAMPOLINE_PHYS(addr) (addr - trampoline_start + TRAMPOLINE_BASE_ADDR)

%define TRAMPOLINE_ADDR(addr) (TRAMPOLINE_BASE_ADDR + (addr))

TRAMPOLINE_BASE_ADDR equ 0x8000
TRAMPOLINE_PML4_OFFSET equ 0x0FF0
TRAMPOLINE_STACK_OFFSET equ 0x0FE8
TRAMPOLINE_CPU_ID_OFFSET equ 0x0FD8

CR0_PE equ (1 << 0)
CR0_PG equ (1 << 31)
CR4_PAE equ (1 << 5)
EFER_LME equ (1 << 8)

GDT_CODE32_SEL equ 0x08
GDT_DATA32_SEL equ 0x10
GDT_CODE64_SEL equ 0x08
GDT_DATA64_SEL equ 0x10

MSR_EFER equ 0xC0000080

section .trampoline
align 16

global trampoline_start
global trampoline_end

[bits 16]
trampoline_start:
    cli
    cld

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    o32 lgdt [TRAMPOLINE_PHYS(protected_mode_gdtr)]

    mov eax, cr0
    or eax, CR0_PE
    mov cr0, eax

    jmp GDT_CODE32_SEL:(TRAMPOLINE_PHYS(trampoline_protected_mode_entry))

[bits 32]
trampoline_protected_mode_entry:
    mov bx, GDT_DATA32_SEL
    mov ds, bx
    mov es, bx
    mov ss, bx

    mov eax, cr4
    or eax, CR4_PAE
    mov cr4, eax

    mov eax, [TRAMPOLINE_ADDR(TRAMPOLINE_PML4_OFFSET)]
    mov cr3, eax

    mov ecx, MSR_EFER
    rdmsr
    or eax, EFER_LME
    wrmsr

    mov eax, cr0
    or eax, CR0_PG
    mov cr0, eax

    lgdt [TRAMPOLINE_PHYS(long_mode_gdtr)]

    jmp GDT_CODE64_SEL:(TRAMPOLINE_PHYS(trampoline_long_mode_entry))

[bits 64]
trampoline_long_mode_entry:
    mov ax, GDT_DATA64_SEL
    mov ds, ax
    mov es, ax
    mov ss, ax

    xor ax, ax
    mov fs, ax
    mov gs, ax

    mov rsp, [TRAMPOLINE_ADDR(TRAMPOLINE_STACK_OFFSET)]
    xor rbp, rbp

    push 0x0
    popfq

    ; init_other_cpu([TRAMPOLINE_ADDR(TRAMPOLINE_CPU_ID_OFFSET)])
    mov rdi, [TRAMPOLINE_ADDR(TRAMPOLINE_CPU_ID_OFFSET)]
    call init_other_cpu

    ; r12 = sched_thread()
    call sched_thread
    mov r12, rax

    ; Allocate buffer for interrupt frame
    sub rsp, INTERRUPT_FRAME_SIZE

    ; thread_load(r12, rsp)
    mov rdi, r12
    mov rsi, rsp
    call thread_load

    INTERRUPT_FRAME_POP_AND_JUMP
    ud2 ; Should never be reached

align 16
protected_mode_gdtr:
    dw protected_mode_gdt_end - protected_mode_gdt_start - 1
    dd TRAMPOLINE_PHYS(protected_mode_gdt_start)
align 16
protected_mode_gdt_start:
    dq 0
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
protected_mode_gdt_end:

align 16
long_mode_gdtr:
    dw long_mode_gdt_end - long_mode_gdt_start - 1
    dq TRAMPOLINE_PHYS(long_mode_gdt_start)
align 16
long_mode_gdt_start:
    dq 0
    dq 0x00AF98000000FFFF
    dq 0x00CF92000000FFFF
long_mode_gdt_end:

align 16
trampoline_end:

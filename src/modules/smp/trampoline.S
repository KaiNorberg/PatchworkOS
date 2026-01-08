.code64
.intel_syntax noprefix

#include "kernel/cpu/interrupt.h"
#include "trampoline.h"

.extern trampoline_c_entry

#define TRAMPOLINE_PHYS(addr) (addr - trampoline_start + TRAMPOLINE_BASE_ADDR)

#define TRAMPOLINE_ADDR(addr) (TRAMPOLINE_BASE_ADDR + (addr))

.set CR0_PE, (1 << 0)
.set CR0_PG, (1 << 31)
.set CR4_PAE, (1 << 5)
.set EFER_LME, (1 << 8)

.set GDT_CODE32_SEL, 0x08
.set GDT_DATA32_SEL, 0x10
.set GDT_CODE64_SEL, 0x08
.set GDT_DATA64_SEL, 0x10

.set MSR_EFER, 0xC0000080

.global trampoline_start
.type trampoline_start, @function
.global trampoline_end
.type trampoline_end, @function

.text

.code16
.align 16
trampoline_start:    
    cli
    cld

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    data32 lgdt [TRAMPOLINE_PHYS(protected_mode_gdtr)]

    mov eax, cr0
    or eax, CR0_PE
    mov cr0, eax

    data32 jmp GDT_CODE32_SEL, TRAMPOLINE_PHYS(trampoline_protected_mode_entry)

.code32
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
    jmp GDT_CODE64_SEL, TRAMPOLINE_PHYS(trampoline_long_mode_entry)

.code64
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

    mov rdi, [TRAMPOLINE_ADDR(TRAMPOLINE_CPU_OFFSET)]
    jmp qword ptr [TRAMPOLINE_ADDR(TRAMPOLINE_ENTRY_OFFSET)]

.align 16
protected_mode_gdtr:
    .short protected_mode_gdt_end - protected_mode_gdt_start - 1
    .long TRAMPOLINE_PHYS(protected_mode_gdt_start)
.align 16
protected_mode_gdt_start:
    .quad 0
    .quad 0x00CF9A000000FFFF
    .quad 0x00CF92000000FFFF
protected_mode_gdt_end:

.align 16
long_mode_gdtr:
    .short long_mode_gdt_end - long_mode_gdt_start - 1
    .quad TRAMPOLINE_PHYS(long_mode_gdt_start)
.align 16
long_mode_gdt_start:
    .quad 0
    .quad 0x00AF98000000FFFF
    .quad 0x00CF92000000FFFF
long_mode_gdt_end:

.align 16
trampoline_end:
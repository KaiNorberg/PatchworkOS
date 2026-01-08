.code64

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

    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    data32 lgdt TRAMPOLINE_PHYS(protected_mode_gdtr)

    movl %cr0, %eax
    orl $CR0_PE, %eax
    movl %eax, %cr0

    data32 ljmp $GDT_CODE32_SEL, $TRAMPOLINE_PHYS(trampoline_protected_mode_entry)

.code32
trampoline_protected_mode_entry:
    movw $GDT_DATA32_SEL, %bx
    movw %bx, %ds
    movw %bx, %es
    movw %bx, %ss

    movl %cr4, %eax
    orl $CR4_PAE, %eax
    movl %eax, %cr4

    movl TRAMPOLINE_ADDR(TRAMPOLINE_PML4_OFFSET), %eax
    movl %eax, %cr3

    movl $MSR_EFER, %ecx
    rdmsr
    orl $EFER_LME, %eax
    wrmsr

    movl %cr0, %eax
    orl $CR0_PG, %eax
    movl %eax, %cr0

    lgdt TRAMPOLINE_PHYS(long_mode_gdtr)
    ljmp $GDT_CODE64_SEL, $TRAMPOLINE_PHYS(trampoline_long_mode_entry)

.code64
trampoline_long_mode_entry:
    movw $GDT_DATA64_SEL, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    xorw %ax, %ax
    movw %ax, %fs
    movw %ax, %gs

    movq TRAMPOLINE_ADDR(TRAMPOLINE_STACK_OFFSET), %rsp
    xorq %rbp, %rbp

    pushq $0x0
    popfq

    movq TRAMPOLINE_ADDR(TRAMPOLINE_CPU_OFFSET), %rdi
    jmp *TRAMPOLINE_ADDR(TRAMPOLINE_ENTRY_OFFSET)

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
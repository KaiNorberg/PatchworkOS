%define PHYSICAL_ADDRESS(address) address - trampoline_virtual_start + TRAMPOLINE_PHYSICAL_START

TRAMPOLINE_PHYSICAL_START equ 0x8000
TRAMPOLINE_PML_ADDRESS equ 0x8FF0
TRAMPOLINE_STACK_ADDRESS equ 0x8FE0
TRAMPOLINE_ENTRY_ADDRESS equ 0x8FD0

section .trampoline
[bits 16]
global trampoline_virtual_start
trampoline_virtual_start:
    cli
    mov ax, 0x0
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    o32 lgdt [PHYSICAL_ADDRESS(protected_mode_gdtr)]

    mov eax, cr0
    or al, 0x1
    mov cr0, eax

    jmp 0x8:(PHYSICAL_ADDRESS(trampoline_protected_mode_entry))

[bits 32]
trampoline_protected_mode_entry:
    mov bx, 0x10
    mov ds, bx
    mov es, bx
    mov ss, bx

    mov eax, cr4
    or  eax, (1 << 5)
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or  eax, (1 << 8)
    wrmsr

    mov eax, dword [TRAMPOLINE_PML_ADDRESS]
    mov cr3, eax

    mov edx, cr0
    or eax, (1 << 31) | (1 << 0)
    mov cr0, eax

    lgdt [PHYSICAL_ADDRESS(long_mode_gdtr)]

    jmp 0x8:(PHYSICAL_ADDRESS(trampoline_long_mode_entry))

[bits 64]
trampoline_long_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov ax, 0x0
    mov fs, ax
    mov gs, ax

    mov rsp, [TRAMPOLINE_STACK_ADDRESS]
    xor rbp, rbp

    push 0x0
    popfq

    jmp [TRAMPOLINE_ENTRY_ADDRESS]

align 16
protected_mode_gdtr:
    dw protected_mode_gdt_end - protected_mode_gdt_start - 1
    dd PHYSICAL_ADDRESS(protected_mode_gdt_start)
align 16
protected_mode_gdt_start:
    dq 0
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
protected_mode_gdt_end:

align 16
long_mode_gdtr:
    dw long_mode_gdt_end - long_mode_gdt_start - 1
    dq PHYSICAL_ADDRESS(long_mode_gdt_start)
align 16
long_mode_gdt_start:
    dq 0
    dq 0x00AF98000000FFFF
    dq 0x00CF92000000FFFF
long_mode_gdt_end:

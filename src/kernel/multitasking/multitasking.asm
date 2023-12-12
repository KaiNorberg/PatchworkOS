global jump_to_user_space

codeSegment equ 0x18 | 3
dataSegment equ 0x20 | 3

; rdi = address to jump to
; rsi = address of task stack top
jump_to_user_space:
	mov ax, dataSegment
	mov ds, ax
	mov es, ax 
	mov fs, ax 
	mov gs, ax
 
	push qword dataSegment
	push qword rsi
	pushfq
	push qword codeSegment
	push qword rdi
	iretq
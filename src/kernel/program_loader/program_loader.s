[bits 64]

section .program_loader

extern program_loader_load

;rdi = executable
global program_loader_entry
program_loader_entry:
    call program_loader_load
    jmp rax

;Janky but it avoids code duplication
%define .text .program_loader
%include "system/functions/exit.s"
%include "system/functions/status.s"
%include "system/functions/map.s"
%include "system/functions/open.s"
%include "system/functions/close.s"
%include "system/functions/read.s"
%include "system/functions/seek.s"
%include "system/functions/sys_test.s"
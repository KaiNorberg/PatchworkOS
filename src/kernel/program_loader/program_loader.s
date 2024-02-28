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
%include "asym/functions/exit.s"
%include "asym/functions/status.s"
%include "asym/functions/map.s"
%include "asym/functions/open.s"
%include "asym/functions/close.s"
%include "asym/functions/read.s"
%include "asym/functions/seek.s"
%include "asym/functions/sys_test.s"
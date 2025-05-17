section .init
global _init
_init:
   push rbp
   mov rbp, rsp

section .fini
global _fini
_fini:
   push rbp
   mov rbp, rsp
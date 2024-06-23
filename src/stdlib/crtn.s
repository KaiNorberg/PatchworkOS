%ifndef __EMBED__

section .init
   pop rbp
   ret

section .fini
   pop rbp
   ret

%endif

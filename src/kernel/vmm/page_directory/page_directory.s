[bits 64]

section .text

;rdi = virtual address
global page_directory_invalidate_page
page_directory_invalidate_page:
    invlpg [rdi]
    ret
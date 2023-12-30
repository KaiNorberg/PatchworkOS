[bits 64]

global page_directory_invalidate_page
global test_func

section .text

;rdi = virtual address
page_directory_invalidate_page:
    invlpg [rdi]
    ret
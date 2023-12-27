[bits 64]

global page_directory_invalidate_page

;rdi = virtual address
page_directory_invalidate_page:
    invlpg [rdi]
    ret
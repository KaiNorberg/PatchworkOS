#include "common/elf.h"

void elf64_load_segments(const Elf64_File* elf, Elf64_Addr base, Elf64_Off offset)
{
    if (elf == NULL)
    {
        return;
    }

    Elf64_Ehdr* header = (Elf64_Ehdr*)elf->header;
    for (uint32_t i = 0; i < header->e_phnum; i++)
    {
        Elf64_Phdr* phdr = ELF64_GET_PHDR(elf, i);
        if (phdr->p_type != PT_LOAD)
        {
            continue;
        }

        void* dest = (void*)(base + (phdr->p_vaddr - offset));
        void* src = ELF64_AT_OFFSET(elf, phdr->p_offset);
        elf_memcpy(dest, src, phdr->p_filesz);
        if (phdr->p_memsz > phdr->p_filesz)
        {
            elf_memset((void*)((uintptr_t)dest + phdr->p_filesz), 0, phdr->p_memsz - phdr->p_filesz);
        }
    }
}
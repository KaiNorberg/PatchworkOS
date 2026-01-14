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

        // Dont use memcpy or memset here since the bootloader uses this file
        uint8_t* dest = (uint8_t*)(base + (phdr->p_vaddr - offset));
        uint8_t* src = ELF64_AT_OFFSET(elf, phdr->p_offset);
        uint64_t j = 0;
        for (; j < phdr->p_filesz; j++)
        {
            dest[j] = src[j];
        }
        for (; j < phdr->p_memsz; j++)
        {
            dest[j] = 0;
        }
    }
}
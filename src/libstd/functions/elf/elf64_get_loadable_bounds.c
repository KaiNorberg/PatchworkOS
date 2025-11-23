#include "common/elf.h"

void elf64_get_loadable_bounds(const Elf64_File* elf, Elf64_Addr* minAddr, Elf64_Addr* maxAddr)
{
    if (elf == NULL || minAddr == NULL || maxAddr == NULL)
    {
        return;
    }

    Elf64_Ehdr* header = (Elf64_Ehdr*)elf->header;

    Elf64_Addr minVaddr = UINT64_MAX;
    Elf64_Addr maxVaddr = 0;
    for (uint32_t i = 0; i < header->e_phnum; i++)
    {
        Elf64_Phdr* phdr = ELF64_GET_PHDR(elf, i);
        if (phdr->p_type == PT_LOAD)
        {
            if (phdr->p_vaddr < minVaddr)
            {
                minVaddr = phdr->p_vaddr;
            }
            if (phdr->p_vaddr + phdr->p_memsz > maxVaddr)
            {
                maxVaddr = phdr->p_vaddr + phdr->p_memsz;
            }
        }
    }

    *minAddr = minVaddr;
    *maxAddr = maxVaddr;
}
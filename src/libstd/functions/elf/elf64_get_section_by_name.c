#include "common/elf.h"

Elf64_Shdr* elf64_get_section_by_name(const Elf64_File* elf, const char* name)
{
    if (elf == NULL || name == NULL)
    {
        return NULL;
    }

    Elf64_Ehdr* header = (Elf64_Ehdr*)elf->header;

    uint64_t shstrndx = header->e_shstrndx;
    if (shstrndx == SHN_XINDEX)
    {
        Elf64_Shdr* firstShdr = ELF64_GET_SHDR(elf, 0);
        shstrndx = firstShdr->sh_link;
    }

    if (shstrndx == SHN_UNDEF)
    {
        return NULL;
    }

    for (uint64_t i = 0; i < header->e_shnum; i++)
    {
        Elf64_Shdr* shdr = ELF64_GET_SHDR(elf, i);
        const char* sectionName = elf64_get_string(elf, shstrndx, shdr->sh_name);
        if (sectionName != NULL && elf_strcmp(sectionName, name) == 0)
        {
            return shdr;
        }
    }

    return NULL;
}
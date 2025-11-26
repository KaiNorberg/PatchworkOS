#include "common/elf.h"

const char* elf64_get_section_name(const Elf64_File* elf, const Elf64_Shdr* section)
{
    if (elf == NULL || section == NULL)
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

    return elf64_get_string(elf, shstrndx, section->sh_name);
}
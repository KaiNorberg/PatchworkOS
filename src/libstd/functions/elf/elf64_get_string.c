#include "common/elf.h"

const char* elf64_get_string(const Elf64_File* elf, Elf64_Xword strTabIndex, Elf64_Off offset)
{
    if (elf == NULL)
    {
        return NULL;
    }

    Elf64_Ehdr* header = (Elf64_Ehdr*)elf->header;
    if (strTabIndex >= header->e_shnum)
    {
        return NULL;
    }

    Elf64_Shdr* strtabHdr = ELF64_GET_SHDR(elf, strTabIndex);
    if (strtabHdr->sh_type != SHT_STRTAB)
    {
        return NULL;
    }

    if (offset >= strtabHdr->sh_size)
    {
        return NULL;
    }

    char* strTable = (char*)ELF64_AT_OFFSET(elf, strtabHdr->sh_offset);
    return &strTable[offset];
}
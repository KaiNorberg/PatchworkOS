#include "common/elf.h"

const char* elf64_get_symbol_name(const Elf64_File* elf, const Elf64_Sym* symbol)
{
    if (elf == NULL || symbol == NULL)
    {
        return NULL;
    }

    if (elf->symtab == NULL)
    {
        return NULL;
    }

    Elf64_Shdr* strtabHdr = ELF64_GET_SHDR(elf, elf->symtab->sh_link);
    char* strTable = ELF64_AT_OFFSET(elf, strtabHdr->sh_offset);
    return &strTable[symbol->st_name];
}
#include "common/elf.h"

Elf64_Sym* elf64_get_symbol_by_name(const Elf64_File* elf, const char* name)
{
    if (elf == NULL || name == NULL)
    {
        return NULL;
    }

    if (elf->symtab == NULL)
    {
        return NULL;
    }

    uint64_t symCount = elf->symtab->sh_size / elf->symtab->sh_entsize;
    void* symTableBase = ELF64_AT_OFFSET(elf, elf->symtab->sh_offset);
    Elf64_Shdr* strtabHdr = ELF64_GET_SHDR(elf, elf->symtab->sh_link);
    char* strTable = ELF64_AT_OFFSET(elf, strtabHdr->sh_offset);

    for (uint64_t i = 0; i < symCount; i++)
    {
        Elf64_Sym* symbol = (Elf64_Sym*)((uintptr_t)symTableBase + (i * elf->symtab->sh_entsize));
        const char* symbolName = &strTable[symbol->st_name];
        if (strcmp(symbolName, name) == 0)
        {
            return symbol;
        }
    }

    return NULL;
}
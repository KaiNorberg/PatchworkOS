#include "common/elf.h"

Elf64_Sym* elf64_get_symbol_by_index(const Elf64_File* elf, Elf64_Xword symbolIndex)
{
    if (elf == NULL)
    {
        return NULL;
    }

    if (elf->symtab == NULL)
    {
        return NULL;
    }

    uint64_t symCount = elf->symtab->sh_size / elf->symtab->sh_entsize;
    if (symbolIndex >= symCount)
    {
        return NULL;
    }

    void* symTableBase = ELF64_AT_OFFSET(elf, elf->symtab->sh_offset);
    return (Elf64_Sym*)((uintptr_t)symTableBase + (symbolIndex * elf->symtab->sh_entsize));
}
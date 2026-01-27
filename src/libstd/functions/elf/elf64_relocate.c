#include "common/elf.h"

bool elf64_relocate(const Elf64_File* elf, Elf64_Addr base, Elf64_Off offset,
    void* (*resolve_symbol)(const char* name, void* data), void* data)
{
    for (uint64_t i = 0; i < elf->header->e_shnum; i++)
    {
        Elf64_Shdr* shdr = ELF64_GET_SHDR(elf, i);
        if (shdr->sh_type != SHT_RELA)
        {
            continue;
        }

        Elf64_Shdr* symtabShdr = ELF64_GET_SHDR(elf, shdr->sh_link);
        void* symTableBase = ELF64_AT_OFFSET(elf, symtabShdr->sh_offset);
        uint64_t symCount = symtabShdr->sh_size / symtabShdr->sh_entsize;

        Elf64_Rela* rela = ELF64_AT_OFFSET(elf, shdr->sh_offset);
        uint64_t relaCount = shdr->sh_size / sizeof(Elf64_Rela);

        for (uint64_t j = 0; j < relaCount; j++)
        {
            Elf64_Addr* patchAddr = (Elf64_Addr*)(base + (rela[j].r_offset - offset));
            Elf64_Xword type = ELF64_R_TYPE(rela[j].r_info);
            Elf64_Xword symIndex = ELF64_R_SYM(rela[j].r_info);

            if (symIndex >= symCount)
            {
                return false;
            }
            Elf64_Sym* sym = (Elf64_Sym*)((uintptr_t)symTableBase + (symIndex * symtabShdr->sh_entsize));
            const char* symName = elf64_get_string(elf, symtabShdr->sh_link, sym->st_name);

            Elf64_Addr value = sym->st_shndx != SHN_UNDEF ? sym->st_value : 0;

            switch (type)
            {
            case R_X86_64_64:
                *patchAddr = base + value + rela[j].r_addend;
                break;
            case R_X86_64_PC32:
                *patchAddr = base + value + rela[j].r_addend - (Elf64_Addr)patchAddr;
                break;
            case R_X86_64_GLOB_DAT:
            case R_X86_64_JUMP_SLOT:
                if (sym->st_shndx != SHN_UNDEF)
                {
                    *patchAddr = base + value + rela[j].r_addend;
                    break;
                }

                *patchAddr = (uint64_t)resolve_symbol(symName, data);
                if (*patchAddr == 0)
                {
                    return false;
                }
                break;
            case R_X86_64_RELATIVE:
                *patchAddr = base + rela[j].r_addend;
                break;
            default:
#ifdef _KERNEL_
                LOG_ERR("unsupported relocation type %llu for symbol '%s'\n", type, symName);
#endif
                return false;
            }
        }
    }

    return true;
}
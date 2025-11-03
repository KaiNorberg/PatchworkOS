#include <errno.h>
#include <stddef.h>
#include <sys/elf.h>

// Makes sure both the kernel and bootloader can use the elf functions
#ifdef _BOOT_
#include <efi.h>
#include <efilib.h>

#define elf_strcmp(Str1, Str2) strcmpa((Str1), (Str2))
#define elf_memcpy(Dest, Src, Size) CopyMem((Dest), (Src), (Size))
#define elf_memset(Dest, Value, Size) SetMem((Dest), (Size), (Value))

static void* elf_memchr(const void* ptr, int value, size_t num)
{
    const unsigned char* p = (const unsigned char*)ptr;
    for (size_t i = 0; i < num; i++)
    {
        if (p[i] == (unsigned char)value)
        {
            return (void*)&p[i];
        }
    }
    return NULL;
}

#else

#include <string.h>

#define elf_strcmp strcmp
#define elf_memcpy memcpy
#define elf_memset memset
#define elf_memchr memchr

#endif

uint64_t elf64_validate(Elf64_File* elf, void* data, uint64_t size)
{
    // This is a big function, but all it does just verify that every single thing that i can think of is as it should
    // be.
    if (elf == NULL || data == NULL || size < sizeof(Elf64_Ehdr))
    {
        return 200;
    }

    elf->symtab = NULL;
    elf->dynsym = NULL;

    Elf64_Ehdr* header = (Elf64_Ehdr*)data;
    if (header->e_ident[EI_MAG0] != ELFMAG0 || header->e_ident[EI_MAG1] != ELFMAG1 ||
        header->e_ident[EI_MAG2] != ELFMAG2 || header->e_ident[EI_MAG3] != ELFMAG3)
    {
        return 1;
    }

    if (header->e_ident[EI_CLASS] != ELFCLASS64)
    {
        return 2;
    }

    if (header->e_ident[EI_DATA] != ELFDATALSB)
    {
        return 3;
    }

    if (header->e_ident[EI_VERSION] != EV_CURRENT || header->e_version != EV_CURRENT)
    {
        return 4;
    }

    if (header->e_ident[EI_OSABI] != ELFOSABI_NONE && header->e_ident[EI_OSABI] != ELFOSABI_GNU)
    {
        return 5;
    }

    if (header->e_shnum > 0 && header->e_shentsize > UINT64_MAX / header->e_shnum)
    {
        return 6;
    }
    if (header->e_shoff > size || (header->e_shentsize * header->e_shnum) > size - header->e_shoff)
    {
        return 7;
    }
    if (header->e_shnum > 0 && header->e_shentsize < sizeof(Elf64_Shdr))
    {
        return 8;
    }

    if (header->e_phnum > 0 && header->e_phentsize > UINT64_MAX / header->e_phnum)
    {
        return 9;
    }
    if (header->e_phoff > size || (header->e_phentsize * header->e_phnum) > size - header->e_phoff)
    {
        return 10;
    }
    if (header->e_phnum > 0 && header->e_phentsize < sizeof(Elf64_Phdr))
    {
        return 11;
    }

    Elf64_Shdr* shstrHdr = NULL;
    uint64_t shstrndx = header->e_shstrndx;

    if (shstrndx == SHN_XINDEX)
    {
        if (header->e_shnum == 0)
        {
            return 12;
        }
        Elf64_Shdr* firstShdr = (Elf64_Shdr*)((uintptr_t)data + header->e_shoff);
        shstrndx = firstShdr->sh_link;
    }

    if (shstrndx != SHN_UNDEF)
    {
        if (shstrndx >= header->e_shnum)
        {
            return 13;
        }
        shstrHdr = (Elf64_Shdr*)((uintptr_t)data + header->e_shoff + (shstrndx * header->e_shentsize));

        if (shstrHdr->sh_type != SHT_STRTAB)
        {
            return 14;
        }

        if (shstrHdr->sh_offset > size || shstrHdr->sh_size > size - shstrHdr->sh_offset)
        {
            return 15;
        }

        if (shstrHdr->sh_size == 0)
        {
            return 16;
        }
        char* strTable = (char*)((uintptr_t)data + shstrHdr->sh_offset);
        if (strTable[shstrHdr->sh_size - 1] != '\0')
        {
            return 17;
        }
    }

    uint64_t symtabCount = 0;
    uint64_t dynsymCount = 0;
    for (uint64_t i = 0; i < header->e_shnum; i++)
    {
        Elf64_Shdr* shdr = (Elf64_Shdr*)((uintptr_t)data + header->e_shoff + (i * header->e_shentsize));

        if (shdr->sh_type != SHT_NOBITS && (shdr->sh_offset > size || shdr->sh_size > size - shdr->sh_offset))
        {
            return 18;
        }

        if (shstrHdr == NULL && shdr->sh_name != 0)
        {
            return 19;
        }
        if (shstrHdr != NULL && shdr->sh_name >= shstrHdr->sh_size)
        {
            return 20;
        }

        switch (shdr->sh_type)
        {
        case SHT_STRTAB:
            if (shdr->sh_size == 0)
            {
                return 21;
            }
            char* strTable = (char*)((uintptr_t)data + shdr->sh_offset);
            if (strTable[shdr->sh_size - 1] != '\0')
            {
                return 22;
            }
            break;
        case SHT_SYMTAB:
        case SHT_DYNSYM:
            if (shdr->sh_type == SHT_SYMTAB)
            {
                symtabCount++;
                elf->symtab = shdr;
            }
            else
            {
                dynsymCount++;
                elf->dynsym = shdr;
            }
            if (dynsymCount > 1)
            {
                return 23;
            }
            if (symtabCount > 1)
            {
                return 24;
            }

            if (shdr->sh_entsize < sizeof(Elf64_Sym))
            {
                return 25;
            }
            if (shdr->sh_size % shdr->sh_entsize != 0)
            {
                return 26;
            }

            if (shdr->sh_link >= header->e_shnum)
            {
                return 27;
            }
            Elf64_Shdr* strtabHdr =
                (Elf64_Shdr*)((uintptr_t)data + header->e_shoff + (shdr->sh_link * header->e_shentsize));
            if (strtabHdr->sh_type != SHT_STRTAB)
            {
                return 28;
            }

            uint64_t symCount = shdr->sh_size / shdr->sh_entsize;
            void* symTableBase = (void*)((uintptr_t)data + shdr->sh_offset);
            for (uint64_t j = 0; j < symCount; j++)
            {
                Elf64_Sym* currentSym = (Elf64_Sym*)((uintptr_t)symTableBase + (j * shdr->sh_entsize));
                if (currentSym->st_name >= strtabHdr->sh_size)
                {
                    return 29;
                }
            }
            break;
        case SHT_RELA:
            if (shdr->sh_entsize < sizeof(Elf64_Rela))
            {
                return 30;
            }
            if (shdr->sh_size % shdr->sh_entsize != 0)
            {
                return 31;
            }
            if (shdr->sh_link >= header->e_shnum)
            {
                return 32;
            }
            Elf64_Shdr* symtabHdr =
                (Elf64_Shdr*)((uintptr_t)data + header->e_shoff + (shdr->sh_link * header->e_shentsize));
            if (symtabHdr->sh_type != SHT_SYMTAB && symtabHdr->sh_type != SHT_DYNSYM)
            {
                return 33;
            }
            if (shdr->sh_info >= header->e_shnum)
            {
                return 34;
            }
            break;
        case SHT_REL:
            if (shdr->sh_entsize < sizeof(Elf64_Rel))
            {
                return 35;
            }
            if (shdr->sh_size % shdr->sh_entsize != 0)
            {
                return 36;
            }
            if (shdr->sh_link >= header->e_shnum)
            {
                return 37;
            }
            Elf64_Shdr* symtabHdrRel =
                (Elf64_Shdr*)((uintptr_t)data + header->e_shoff + (shdr->sh_link * header->e_shentsize));
            if (symtabHdrRel->sh_type != SHT_SYMTAB && symtabHdrRel->sh_type != SHT_DYNSYM)
            {
                return 38;
            }
            if (shdr->sh_info >= header->e_shnum)
            {
                return 39;
            }
            break;
        default:
            break;
        }
    }

    for (uint64_t i = 0; i < header->e_phnum; i++)
    {
        Elf64_Phdr* phdr = (Elf64_Phdr*)((uintptr_t)data + header->e_phoff + (i * header->e_phentsize));
        if (phdr->p_offset > size || phdr->p_filesz > size - phdr->p_offset)
        {
            return 40;
        }
        switch (phdr->p_type)
        {
        case PT_LOAD:
            if (phdr->p_memsz < phdr->p_filesz)
            {
                return 41;
            }
            break;
        case PT_INTERP:
            if (phdr->p_filesz == 0)
            {
                return 42;
            }
            unsigned char* interpData = (unsigned char*)((uintptr_t)data + phdr->p_offset);
            if (elf_memchr(interpData, '\0', phdr->p_filesz) == NULL)
            {
                return 43;
            }
            break;
        case PT_PHDR:
            if (phdr->p_offset != header->e_phoff || phdr->p_filesz != (uint64_t)header->e_phnum * header->e_phentsize)
            {
                return 44;
            }
            break;
        default:
            break;
        }
    }

    elf->header = data;
    elf->size = size;
    return 0;
}

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

Elf64_Sym* elf64_get_dynamic_symbol_by_index(const Elf64_File* elf, Elf64_Xword symbolIndex)
{
    if (elf == NULL)
    {
        return NULL;
    }

    if (elf->dynsym == NULL)
    {
        return NULL;
    }

    uint64_t symCount = elf->dynsym->sh_size / elf->dynsym->sh_entsize;
    if (symbolIndex >= symCount)
    {
        return NULL;
    }

    void* symTableBase = ELF64_AT_OFFSET(elf, elf->dynsym->sh_offset);
    return (Elf64_Sym*)((uintptr_t)symTableBase + (symbolIndex * elf->dynsym->sh_entsize));
}

const char* elf64_get_dynamic_symbol_name(const Elf64_File* elf, const Elf64_Sym* symbol)
{
    if (elf == NULL || symbol == NULL)
    {
        return NULL;
    }

    if (elf->dynsym == NULL)
    {
        return NULL;
    }

    Elf64_Shdr* strtabHdr = ELF64_GET_SHDR(elf, elf->dynsym->sh_link);
    char* strTable = ELF64_AT_OFFSET(elf, strtabHdr->sh_offset);
    return &strTable[symbol->st_name];
}

uint64_t elf64_relocate(const Elf64_File* elf, Elf64_Addr base, Elf64_Off offset,
    Elf64_Addr (*resolve_symbol)(const char* name, void* private), void* private)
{
    for (uint64_t i = 0; i < elf->header->e_shnum; i++)
    {
        Elf64_Shdr* shdr = ELF64_GET_SHDR(elf, i);
        if (shdr->sh_type != SHT_RELA)
        {
            continue;
        }

        const char* sectionName = elf64_get_section_name(elf, shdr);

        Elf64_Shdr* targetShdr = ELF64_GET_SHDR(elf, shdr->sh_info);

        Elf64_Rela* rela = ELF64_AT_OFFSET(elf, shdr->sh_offset);
        uint64_t relaCount = shdr->sh_size / sizeof(Elf64_Rela);

        for (uint64_t j = 0; j < relaCount; j++)
        {
            Elf64_Addr* patchAddr = (Elf64_Addr*)(base + (targetShdr->sh_addr + rela[j].r_offset - offset));
            Elf64_Xword type = ELF64_R_TYPE(rela[j].r_info);
            Elf64_Xword symIndex = ELF64_R_SYM(rela[j].r_info);

            Elf64_Sym* sym = elf64_get_dynamic_symbol_by_index(elf, symIndex);
            const char* symName = elf64_get_dynamic_symbol_name(elf, sym);

            if (sym->st_shndx == SHN_UNDEF)
            {
                Elf64_Addr symAddr = resolve_symbol(symName, private);
                if (symAddr == 0)
                {
                    return ERR;
                }

                switch (type)
                {
                case R_X86_64_GLOB_DAT:
                case R_X86_64_JUMP_SLOT:
                    *patchAddr = symAddr;
                    break;
                case R_X86_64_RELATIVE:
                    *patchAddr = base + rela[j].r_addend;
                    break;
                default:
                    return ERR;
                }

                continue;
            }

            switch (type)
            {
            case R_X86_64_RELATIVE:
                *patchAddr = base + rela[j].r_addend;
                break;
            default:
                return ERR;
            }
        }
    }

    return 0;
}

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/elf.h>

uint64_t elf_file_validate(Elf64_File* elf, void* data, uint64_t size)
{
    // This is a big function, but all it does just verify that every single thing that i can think of is as it should
    // be.
    if (elf == NULL || data == NULL || size < sizeof(Elf64_Ehdr))
    {
        return 1;
    }

    Elf64_Ehdr* header = (Elf64_Ehdr*)data;
    if (header->e_ident[EI_MAG0] != ELFMAG0 || header->e_ident[EI_MAG1] != ELFMAG1 ||
        header->e_ident[EI_MAG2] != ELFMAG2 || header->e_ident[EI_MAG3] != ELFMAG3)
    {
        return 2;
    }

    if (header->e_ident[EI_CLASS] != ELFCLASS64)
    {
        return 3;
    }

    if (header->e_ident[EI_DATA] != ELFDATALSB)
    {
        return 4;
    }

    if (header->e_ident[EI_VERSION] != EV_CURRENT || header->e_version != EV_CURRENT)
    {
        return 5;
    }

    if (header->e_ident[EI_OSABI] != ELFOSABI_NONE && header->e_ident[EI_OSABI] != ELFOSABI_GNU)
    {
        return 6;
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
        return ERR;
    }

    if (header->e_phnum > 0 && header->e_phentsize > UINT64_MAX / header->e_phnum)
    {
        return 8;
    }
    if (header->e_phoff > size || (header->e_phentsize * header->e_phnum) > size - header->e_phoff)
    {
        return 9;
    }
    if (header->e_phnum > 0 && header->e_phentsize < sizeof(Elf64_Phdr))
    {
        return 10;
    }

    Elf64_Shdr* shstrHdr = NULL;
    uint64_t shstrndx = header->e_shstrndx;

    if (shstrndx == SHN_XINDEX)
    {
        if (header->e_shnum == 0)
        {
            return 11;
        }
        Elf64_Shdr* firstShdr = (Elf64_Shdr*)((uintptr_t)data + header->e_shoff);
        shstrndx = firstShdr->sh_link;
    }

    if (shstrndx != SHN_UNDEF)
    {
        if (shstrndx >= header->e_shnum)
        {
            return 12;
        }
        shstrHdr = (Elf64_Shdr*)((uintptr_t)data + header->e_shoff + (shstrndx * header->e_shentsize));

        if (shstrHdr->sh_type != SHT_STRTAB)
        {
            return 13;
        }

        if (shstrHdr->sh_offset > size || shstrHdr->sh_size > size - shstrHdr->sh_offset)
        {
            return 14;
        }

        if (shstrHdr->sh_size == 0)
        {
            return 15;
        }
        char* strTable = (char*)((uintptr_t)data + shstrHdr->sh_offset);
        if (strTable[shstrHdr->sh_size - 1] != '\0')
        {
            return 16;
        }
    }

    uint64_t symtabCount = 0;
    uint64_t dynsymCount = 0;
    for (uint64_t i = 0; i < header->e_shnum; i++)
    {
        Elf64_Shdr* shdr = (Elf64_Shdr*)((uintptr_t)data + header->e_shoff + (i * header->e_shentsize));

        if (shdr->sh_type != SHT_NOBITS && (shdr->sh_offset > size || shdr->sh_size > size - shdr->sh_offset))
        {
            return 17;
        }

        if (shstrHdr == NULL && shdr->sh_name != 0)
        {
            return 18;
        }
        if (shstrHdr != NULL && shdr->sh_name >= shstrHdr->sh_size)
        {
            return 19;
        }

        switch (shdr->sh_type)
        {
        case SHT_STRTAB:
            if (shdr->sh_size == 0)
            {
                return 20;
            }
            char* strTable = (char*)((uintptr_t)data + shdr->sh_offset);
            if (strTable[shdr->sh_size - 1] != '\0')
            {
                return 21;
            }
            break;
        case SHT_SYMTAB:
        case SHT_DYNSYM:
            if (shdr->sh_type == SHT_SYMTAB)
            {
                symtabCount++;
            }
            else
            {
                dynsymCount++;
            }
            if (dynsymCount > 1)
            {
                return 22;
            }
            if (symtabCount > 1)
            {
                return 23;
            }

            if (shdr->sh_entsize < sizeof(Elf64_Sym))
            {
                return 24;
            }
            if (shdr->sh_size % shdr->sh_entsize != 0)
            {
                return 25;
            }

            if (shdr->sh_link >= header->e_shnum)
            {
                return 26;
            }
            Elf64_Shdr* strtabHdr =
                (Elf64_Shdr*)((uintptr_t)data + header->e_shoff + (shdr->sh_link * header->e_shentsize));
            if (strtabHdr->sh_type != SHT_STRTAB)
            {
                return 27;
            }

            uint64_t symCount = shdr->sh_size / shdr->sh_entsize;
            void* symTableBase = (void*)((uintptr_t)data + shdr->sh_offset);
            for (uint64_t j = 0; j < symCount; j++)
            {
                Elf64_Sym* currentSym = (Elf64_Sym*)((uintptr_t)symTableBase + (j * shdr->sh_entsize));
                if (currentSym->st_name >= strtabHdr->sh_size)
                {
                    return 28;
                }
            }
            break;
        case SHT_RELA:
            if (shdr->sh_entsize < sizeof(Elf64_Rela))
            {
                return 29;
            }
            if (shdr->sh_size % shdr->sh_entsize != 0)
            {
                return 30;
            }
            if (shdr->sh_link >= header->e_shnum)
            {
                return 31;
            }
            Elf64_Shdr* symtabHdr =
                (Elf64_Shdr*)((uintptr_t)data + header->e_shoff + (shdr->sh_link * header->e_shentsize));
            if (symtabHdr->sh_type != SHT_SYMTAB && symtabHdr->sh_type != SHT_DYNSYM)
            {
                return 32;
            }
            if (shdr->sh_info >= header->e_shnum)
            {
                return 33;
            }
            break;
        case SHT_REL:
            if (shdr->sh_entsize < sizeof(Elf64_Rel))
            {
                return 34;
            }
            if (shdr->sh_size % shdr->sh_entsize != 0)
            {
                return 35;
            }
            if (shdr->sh_link >= header->e_shnum)
            {
                return 36;
            }
            Elf64_Shdr* symtabHdrRel =
                (Elf64_Shdr*)((uintptr_t)data + header->e_shoff + (shdr->sh_link * header->e_shentsize));
            if (symtabHdrRel->sh_type != SHT_SYMTAB && symtabHdrRel->sh_type != SHT_DYNSYM)
            {
                return 37;
            }
            if (shdr->sh_info >= header->e_shnum)
            {
                return 38;
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
            return 39;
        }
        switch (phdr->p_type)
        {
        case PT_LOAD:
            if (phdr->p_memsz < phdr->p_filesz)
            {
                return 40;
            }
            break;
        case PT_INTERP:
            if (phdr->p_filesz == 0)
            {
                return 41;
            }
            unsigned char* interpData = (unsigned char*)((uintptr_t)data + phdr->p_offset);
            if (memchr(interpData, '\0', phdr->p_filesz) == NULL)
            {
                return 42;
            }
            break;
        case PT_PHDR:
            if (phdr->p_offset != header->e_phoff || phdr->p_filesz != (uint64_t)header->e_phnum * header->e_phentsize)
            {
                return 43;
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

void elf_file_get_loadable_bounds(const Elf64_File* elf, Elf64_Addr* minAddr, Elf64_Addr* maxAddr)
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
        Elf64_Phdr* phdr = ELF_FILE_GET_PHDR(elf, i);
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

const char* elf_file_get_string(const Elf64_File* elf, Elf64_Xword strTabIndex, Elf64_Off offset)
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

    Elf64_Shdr* strtabHdr = ELF_FILE_GET_SHDR(elf, strTabIndex);
    if (strtabHdr->sh_type != SHT_STRTAB)
    {
        return NULL;
    }

    if (offset >= strtabHdr->sh_size)
    {
        return NULL;
    }

    char* strTable = (char*)ELF_FILE_AT_OFFSET(elf, strtabHdr->sh_offset);
    return &strTable[offset];
}

bool elf_file_symbol_iterator_next(Elf64_File_Symbol_Iterator* iterator)
{
    if (iterator == NULL)
    {
        return false;
    }

    Elf64_Ehdr* header = (Elf64_Ehdr*)iterator->elf->header;

    while (iterator->currentShdrIndex < header->e_shnum)
    {
        Elf64_Shdr* shdr = ELF_FILE_GET_SHDR(iterator->elf, iterator->currentShdrIndex);
        if (shdr->sh_type == SHT_SYMTAB || shdr->sh_type == SHT_DYNSYM)
        {
            Elf64_Xword symCount = shdr->sh_size / shdr->sh_entsize;
            if (iterator->currentSymbolIndex < symCount)
            {
                void* symTableBase = (void*)ELF_FILE_AT_OFFSET(iterator->elf, shdr->sh_offset);
                iterator->symbol =
                    (Elf64_Sym*)((uintptr_t)symTableBase + (iterator->currentSymbolIndex * shdr->sh_entsize));
                iterator->symbolName =
                    (char*)elf_file_get_string(iterator->elf, shdr->sh_link, iterator->symbol->st_name);
                iterator->currentSymbolIndex++;
                return true;
            }
        }

        iterator->currentShdrIndex++;
        iterator->currentSymbolIndex = 0;
    }

    return false;
}

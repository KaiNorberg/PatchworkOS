#include "_libstd/MAX_PATH.h"
#define ELF_HEADER_INLINE

typedef struct ioring ioring_t;
static ioring_t* _ioring;
#define _IORING_GET() _ioring

#define _IORING_STRLEN(_str) \
    ({ \
        size_t _len = 0; \
        while ((_str)[_len] != '\0') \
            _len++; \
        _len; \
    })

#include <libstd/auxv.h>
#include <libstd/elf.h>
#include <libstd/fs.h>
#include <libstd/io.h>
#include <libstd/math.h>

/**
 * @brief Dynamic Linker
 * @defgroup comp_dynlink Dynamic Linker
 * @ingroup comp
 *
 * @todo Write dynamic linker documentation.
 *
 * @see libstd_elf for information on the ELF file format.
 * @see https://flapenguin.me/elf-dt-gnu-hash for information regarding GNU hashing.
 *
 */

static ioring_t ioring;

#define IORING_BASE ((void*)0x680000000000)
#define FILE_MAP_BASE ((void*)0x600000000000)

typedef struct
{
    void* base;
    fd_t execFd;
} dyn_info_t;

typedef struct dso
{
    struct dso* next;
    const char* name;
    void* base;
    void* entry;
    Elf64_Addr loadOffset;
    Elf64_Dyn* dynamic;
    Elf64_Sym* symtab;
    const char* strtab;
    uint32_t* hash;
    uint32_t* gnuHash;
    Elf64_Rela* rela;
    Elf64_Xword relaSize;
    Elf64_Xword relaEnt;
    Elf64_Rela* jmprel;
    Elf64_Xword jmprelSize;
    void (*init)(void);
    void (**initArray)(void);
    size_t initArraySize;
} dso_t;

#define MAX_DSOS 64
static dso_t dsos[MAX_DSOS];
static size_t dsoAmount = 0;
static dso_t* dsoList = NULL;
static dso_t* dsoTail = NULL;

static dso_t* _dyn_alloc_dso(void)
{
    if (dsoAmount >= MAX_DSOS)
    {
        return NULL;
    }
    dso_t* dso = &dsos[dsoAmount++];
    unsigned char* p = (unsigned char*)dso;
    for (size_t i = 0; i < sizeof(dso_t); i++)
    {
        p[i] = 0;
    }
    return dso;
}

static int _dyn_strcmp(const char* s1, const char* s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static void _dyn_strcpy(char* dest, const char* src)
{
    while (*src)
    {
        *dest++ = *src++;
    }
    *dest = '\0';
}

static void _dyn_memset(void* s, int c, size_t n)
{
    unsigned char* p = (unsigned char*)s;
    while (n--)
    {
        *p++ = (unsigned char)c;
    }
}

static void _dyn_memcpy(void* dest, const void* src, size_t n)
{
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (n--)
    {
        *d++ = *s++;
    }
}

extern Elf64_Dyn _DYNAMIC[] HIDDEN;

static void _dyn_parse_auxv(void* stack, dyn_info_t* info)
{
    uintptr_t* p = (uintptr_t*)stack;
    uintptr_t argc = *p++;
    p += argc; // Skip argv pointers
    p++;       // Skip NULL terminator

    auxv_t* auxv = (auxv_t*)p;
    for (size_t i = 0; auxv[i].type != AUXV_NULL; i++)
    {
        if (auxv[i].type == AUXV_BASE)
        {
            info->base = (void*)auxv[i].value;
        }
        else if (auxv[i].type == AUXV_EXECFD)
        {
            info->execFd = (fd_t)auxv[i].value;
        }
    }
}

static void _dyn_relocate_self(dyn_info_t* info)
{
    Elf64_Rela* rela = NULL;
    Elf64_Xword relaSize = 0;
    Elf64_Xword relaEnt = 0;

    for (size_t i = 0; _DYNAMIC[i].d_tag != DT_NULL; i++)
    {
        if (_DYNAMIC[i].d_tag == DT_RELA)
        {
            rela = (Elf64_Rela*)(info->base + _DYNAMIC[i].d_un.d_ptr);
        }
        else if (_DYNAMIC[i].d_tag == DT_RELASZ)
        {
            relaSize = _DYNAMIC[i].d_un.d_val;
        }
        else if (_DYNAMIC[i].d_tag == DT_RELAENT)
        {
            relaEnt = _DYNAMIC[i].d_un.d_val;
        }
    }

    if (rela != NULL && relaEnt > 0)
    {
        size_t count = relaSize / relaEnt;
        for (size_t i = 0; i < count; i++)
        {
            Elf64_Rela* r = (Elf64_Rela*)((uint8_t*)rela + (i * relaEnt));
            switch (ELF64_R_TYPE(r->r_info))
            {
            case R_X86_64_RELATIVE:
            {
                Elf64_Addr* target = (Elf64_Addr*)(info->base + r->r_offset);
                *target = (Elf64_Addr)info->base + r->r_addend;
            }
            break;
            default:
                break;
            }
        }
    }
}

static void _dyn_print(const char* str)
{
    size_t len = 0;
    while (str[len] != '\0')
    {
        len++;
    }

    iowrite(FDOUT, IOBUF(str, len), IOCUR, NULL);
}

static status_t _dyn_setup_ioring(void)
{
    _ioring = &ioring;
    return ioring_setup(&ioring, IORING_BASE, 16, 16);
}

static uint32_t _dyn_elf_hash(const char* name)
{
    unsigned long h = 0;
    unsigned long g = 0;
    while (*name != '\0')
    {
        h = (h << 4) + *name++;
        if ((g = h & 0xf0000000))
        {
            h ^= g >> 24;
        }
        h &= ~g;
    }
    return h;
}

static uint32_t _dyn_gnu_hash(const char* name)
{
    uint32_t h = 5381;
    for (; *name; name++)
    {
        h = (h << 5) + h + *name;
    }
    return h;
}

static Elf64_Sym* _dyn_lookup_symbol(const char* name, dso_t* start, dso_t** found)
{
    uint32_t sysvHash = 0;
    uint32_t gnuHash = 0;
    bool sysvHashed = false;
    bool gnuHashed = false;

    for (dso_t* dso = start; dso != NULL; dso = dso->next)
    {
        if (dso->symtab == NULL || dso->strtab == NULL)
        {
            continue;
        }

        if (dso->gnuHash != NULL)
        {
            if (!gnuHashed)
            {
                gnuHash = _dyn_gnu_hash(name);
                gnuHashed = true;
            }

            uint32_t nbuckets = dso->gnuHash[0];
            uint32_t symoffset = dso->gnuHash[1];
            uint32_t bloomSize = dso->gnuHash[2];
            uint32_t bloomShift = dso->gnuHash[3];

            uint64_t* bloom = (uint64_t*)&dso->gnuHash[4];
            uint32_t* buckets = (uint32_t*)&bloom[bloomSize];
            uint32_t* chain = &buckets[nbuckets];

            uint64_t word = bloom[(gnuHash / ELF_BITS) % bloomSize];
            uint64_t mask =
                0 | (uint64_t)1 << (gnuHash % ELF_BITS) | (uint64_t)1 << ((gnuHash >> bloomShift) % ELF_BITS);

            if ((word & mask) != mask)
            {
                continue;
            }

            uint32_t symix = buckets[gnuHash % nbuckets];
            if (symix < symoffset)
            {
                continue;
            }

            while (true)
            {
                const char* symname = dso->strtab + dso->symtab[symix].st_name;
                const uint32_t hashVal = chain[symix - symoffset];

                if ((gnuHash | 1) == (hashVal | 1) && _dyn_strcmp(name, symname) == 0 &&
                    dso->symtab[symix].st_shndx != SHN_UNDEF)
                {
                    if ((void*)found != NULL)
                    {
                        *found = dso;
                    }
                    return &dso->symtab[symix];
                }

                if (hashVal & 1)
                {
                    break;
                }

                symix++;
            }
        }
        else if (dso->hash != NULL)
        {
            if (!sysvHashed)
            {
                sysvHash = _dyn_elf_hash(name);
                sysvHashed = true;
            }

            uint32_t nbucket = dso->hash[0];
            uint32_t* bucket = &dso->hash[2];
            uint32_t* chain = &dso->hash[2 + nbucket];

            for (uint32_t i = bucket[sysvHash % nbucket]; i != 0; i = chain[i])
            {
                Elf64_Sym* sym = &dso->symtab[i];
                if (_dyn_strcmp(dso->strtab + sym->st_name, name) == 0)
                {
                    if (sym->st_shndx != SHN_UNDEF)
                    {
                        if ((void*)found != NULL)
                        {
                            *found = dso;
                        }
                        return sym;
                    }
                }
            }
        }
    }
    return NULL;
}

static bool _dyn_do_relocations(dso_t* dso, Elf64_Rela* rela, size_t size, size_t ent)
{
    if (rela == NULL || ent == 0)
    {
        return true;
    }

    size_t count = size / ent;
    for (size_t i = 0; i < count; i++)
    {
        Elf64_Rela* r = (Elf64_Rela*)((uint8_t*)rela + (i * ent));
        Elf64_Xword type = ELF64_R_TYPE(r->r_info);
        Elf64_Xword symIdx = ELF64_R_SYM(r->r_info);
        Elf64_Addr* target = (Elf64_Addr*)(dso->loadOffset + r->r_offset);

        if (type == R_X86_64_RELATIVE)
        {
            *target = dso->loadOffset + r->r_addend;
            continue;
        }

        Elf64_Sym* sym = &dso->symtab[symIdx];
        const char* symName = dso->strtab + sym->st_name;

        dso_t* lookupStart = (type == R_X86_64_COPY) ? dsoList->next : dsoList;
        dso_t* foundDso = NULL;
        Elf64_Sym* foundSym = _dyn_lookup_symbol(symName, lookupStart, &foundDso);

        Elf64_Addr symVal = 0;
        if (foundSym != NULL && foundDso != NULL)
        {
            symVal = foundDso->loadOffset + foundSym->st_value;
        }
        else if (sym->st_shndx != SHN_UNDEF)
        {
            symVal = dso->loadOffset + sym->st_value;
        }
        else if (ELF64_ST_BIND(sym->st_info) != STB_WEAK)
        {
            _dyn_print("dynlink: undefined symbol ");
            _dyn_print(symName);
            _dyn_print("\n");
            return false;
        }

        switch (type)
        {
        case R_X86_64_64:
        case R_X86_64_GLOB_DAT:
        case R_X86_64_JUMP_SLOT:
            *target = symVal + r->r_addend;
            break;
        case R_X86_64_PC32:
            *(uint32_t*)target = (uint32_t)(symVal + r->r_addend - (Elf64_Addr)target);
            break;
        case R_X86_64_COPY:
            if (foundSym != NULL && foundDso != NULL)
            {
                _dyn_memcpy((void*)target, (const void*)symVal, sym->st_size);
            }
            else
            {
                _dyn_print("dynlink: failed to resolve COPY relocation for ");
                _dyn_print(symName);
                _dyn_print("\n");
                return false;
            }
            break;
        default:
            _dyn_print("dynlink: unknown relocation type ");
            _dyn_print(symName);
            _dyn_print("\n");
            return false;
        }
    }

    return true;
}

static bool _dyn_relocate_dso(dso_t* dso)
{
    if (dso->rela != NULL)
    {
        if (!_dyn_do_relocations(dso, dso->rela, dso->relaSize, dso->relaEnt != 0 ? dso->relaEnt : sizeof(Elf64_Rela)))
        {
            return false;
        }
    }
    if (dso->jmprel != NULL)
    {
        if (!_dyn_do_relocations(dso, dso->jmprel, dso->jmprelSize, sizeof(Elf64_Rela)))
        {
            return false;
        }
    }
    return true;
}

static dso_t* _dyn_load_elf(fd_t fd, const char* name, bool isMain)
{
    iovar_t sizeReg = IOREG(IOREG0, SIZE_MAX);
    iovar_t fileMapReg = IOREG(IOREG1, NULL);

    IOATTRQ(fd, FILE_GET_SIZE, 0, IOSOFT, &sizeReg, 0);
    IOMAPQ(fd, FILE_MAP_BASE, sizeReg, 0, IOMAP_READ | IOMAP_EXEC, IONOLINK, &fileMapReg, 0);

    status_t status = iosync();
    if (IS_ERR(status))
    {
        return NULL;
    }

    size_t size = IOREG_LOAD(sizeReg);
    void* fileMap = (void*)IOREG_LOAD(fileMapReg);

    Elf64_File elf;
    if (elf64_validate(&elf, fileMap, size) != 0)
    {
        iounmap(fileMap, size);
        return NULL;
    }

    Elf64_Addr minAddr = UINT64_MAX;
    Elf64_Addr maxAddr = 0;
    elf64_get_loadable_bounds(&elf, &minAddr, &maxAddr);

    if (minAddr > maxAddr)
    {
        iounmap(fileMap, size);
        return NULL;
    }

    size_t loadSize = maxAddr - minAddr;

    iovar_t zeroFdReg = IOREG(IOREG0, NULL);
    iovar_t memMapReg = IOREG(IOREG1, NULL);

    void* memMap = (isMain && elf.header->e_type == ET_EXEC) ? (void*)minAddr : NULL;

    IOWALKQ(FDCWD, FDROOT, "/dev/const/zero:rwx", IOSOFT, &zeroFdReg, 0);
    IOMAPQ(zeroFdReg, memMap, loadSize, 0, IOMAP_READ | IOMAP_WRITE | IOMAP_EXEC, IOHARD, &memMapReg, 0);
    IODROPQ(zeroFdReg, IONOLINK, NULL, 0);

    status = iosync();
    if (IS_ERR(status))
    {
        iounmap(fileMap, size);
        return NULL;
    }

    memMap = (void*)IOREG_LOAD(memMapReg);
    Elf64_Addr loadOffset = (Elf64_Addr)memMap - minAddr;

    for (size_t i = 0; i < elf.header->e_phnum; i++)
    {
        Elf64_Phdr* phdr = ELF64_GET_PHDR(&elf, i);
        if (phdr->p_type != PT_LOAD)
        {
            continue;
        }

        iomap_t mapFlags = 0;
        if (phdr->p_flags & PF_R)
        {
            mapFlags |= IOMAP_READ;
        }
        if (phdr->p_flags & PF_W)
        {
            mapFlags |= IOMAP_WRITE;
        }
        if (phdr->p_flags & PF_X)
        {
            mapFlags |= IOMAP_EXEC;
        }

        Elf64_Addr vaddr = phdr->p_vaddr + loadOffset;
        Elf64_Off offset = phdr->p_offset;

        Elf64_Addr vaddrAligned = ROUND_DOWN(vaddr, PAGE_SIZE);
        Elf64_Off offsetAligned = ROUND_DOWN(offset, PAGE_SIZE);
        size_t diff = vaddr - vaddrAligned;
        size_t mapLen = ROUND_UP(phdr->p_filesz + diff, PAGE_SIZE);

        if (!(mapFlags & IOMAP_WRITE))
        {
            void* mapAddr = (void*)vaddrAligned;
            iomap_t initialFlags = mapFlags;
            if (phdr->p_memsz > phdr->p_filesz)
            {
                initialFlags |= IOMAP_WRITE;
            }

            status = iomap(fd, &mapAddr, mapLen, offsetAligned, initialFlags);
            if (IS_ERR(status))
            {
                iounmap(fileMap, size);
                return NULL;
            }

            if (phdr->p_memsz > phdr->p_filesz)
            {
                size_t zeroStart = vaddr + phdr->p_filesz;
                size_t zeroEnd = vaddrAligned + mapLen;
                if (zeroStart < zeroEnd)
                {
                    _dyn_memset((void*)zeroStart, 0, zeroEnd - zeroStart);
                }
            }

            if (initialFlags != mapFlags)
            {
                ioprotect(mapAddr, mapLen, mapFlags);
            }
            continue;
        }

        if (phdr->p_filesz > 0)
        {
            _dyn_memcpy((void*)vaddr, (const void*)((uintptr_t)fileMap + phdr->p_offset), phdr->p_filesz);
        }

        size_t memszLen = ROUND_UP(phdr->p_memsz + diff, PAGE_SIZE);
        ioprotect((void*)vaddrAligned, memszLen, mapFlags);
    }

    dso_t* dso = _dyn_alloc_dso();
    if (!dso)
    {
        iounmap(memMap, loadSize);
        iounmap(fileMap, size);
        return NULL;
    }

    dso->name = name;
    dso->base = memMap;
    dso->loadOffset = loadOffset;
    dso->entry = (void*)(elf.header->e_entry + loadOffset);

    for (size_t i = 0; i < elf.header->e_phnum; i++)
    {
        Elf64_Phdr* phdr = ELF64_GET_PHDR(&elf, i);
        if (phdr->p_type == PT_DYNAMIC)
        {
            dso->dynamic = (Elf64_Dyn*)((Elf64_Addr)memMap + phdr->p_vaddr - minAddr);
            break;
        }
    }

    if (dso->dynamic != NULL)
    {
        for (size_t i = 0; dso->dynamic[i].d_tag != DT_NULL; i++)
        {
            Elf64_Dyn* d = &dso->dynamic[i];
            Elf64_Addr addr = d->d_un.d_ptr + loadOffset;
            switch (d->d_tag)
            {
            case DT_SYMTAB:
                dso->symtab = (Elf64_Sym*)addr;
                break;
            case DT_STRTAB:
                dso->strtab = (const char*)addr;
                break;
            case DT_HASH:
                dso->hash = (uint32_t*)addr;
                break;
            case DT_GNU_HASH:
                dso->gnuHash = (uint32_t*)addr;
                break;
            case DT_RELA:
                dso->rela = (Elf64_Rela*)addr;
                break;
            case DT_RELASZ:
                dso->relaSize = d->d_un.d_val;
                break;
            case DT_RELAENT:
                dso->relaEnt = d->d_un.d_val;
                break;
            case DT_JMPREL:
                dso->jmprel = (Elf64_Rela*)addr;
                break;
            case DT_PLTRELSZ:
                dso->jmprelSize = d->d_un.d_val;
                break;
            case DT_INIT:
                dso->init = (void (*)(void))addr;
                break;
            case DT_INIT_ARRAY:
                dso->initArray = (void (**)(void))addr;
                break;
            case DT_INIT_ARRAYSZ:
                dso->initArraySize = d->d_un.d_val;
                break;
            default:
                break;
            }
        }
    }

    iounmap(fileMap, size);
    return dso;
}

static void _dyn_load_dependencies(dso_t* mainDso)
{
    dsoList = mainDso;
    dsoTail = mainDso;

    dso_t* curr = dsoList;
    while (curr != NULL)
    {
        if (curr->dynamic == NULL)
        {
            curr = curr->next;
            continue;
        }

        for (size_t i = 0; curr->dynamic[i].d_tag != DT_NULL; i++)
        {
            Elf64_Dyn* d = &curr->dynamic[i];
            if (d->d_tag != DT_NEEDED)
            {
                continue;
            }

            const char* depName = curr->strtab + d->d_un.d_val;

            bool loaded = false;
            for (dso_t* check = dsoList; check != NULL; check = check->next)
            {
                if (_dyn_strcmp(check->name, depName) == 0)
                {
                    loaded = true;
                    break;
                }
            }

            if (loaded)
            {
                continue;
            }

            static char path[MAX_PATH];
            _dyn_strcpy(path, "/lib/");

            size_t len = 5;
            const char* s = depName;
            while (*s && len < sizeof(path) - 1)
            {
                path[len++] = *s++;
            }
            path[len] = '\0';

            fd_t depFd;
            if (!IS_ERR(iowalk(FDCWD, FDROOT, path, &depFd)))
            {
                dso_t* depDso = _dyn_load_elf(depFd, depName, false);
                iodrop(depFd);
                if (depDso != NULL)
                {
                    dsoTail->next = depDso;
                    dsoTail = depDso;
                }
            }
        }
        curr = curr->next;
    }
}

static void _dyn_run_all_inits(void)
{
    for (int i = dsoAmount - 1; i >= 0; i--)
    {
        dso_t* dso = &dsos[i];
        if (dso->init != NULL)
        {
            dso->init();
        }
        if (dso->initArray != NULL)
        {
            size_t count = dso->initArraySize / sizeof(void*);
            for (size_t j = 0; j < count; j++)
            {
                dso->initArray[j]();
            }
        }
    }
}

HIDDEN void* _dyn_main(void* stack)
{
    dyn_info_t info = {0};
    _dyn_parse_auxv(stack, &info);

    _dyn_relocate_self(&info);

    status_t status = _dyn_setup_ioring();
    if (IS_ERR(status))
    {
        return NULL;
    }

    dso_t* mainDso = _dyn_load_elf(info.execFd, "main", true);
    if (mainDso == NULL)
    {
        _dyn_print("dynlink: failed to load executable\n");
        ioring_teardown(&ioring);
        return NULL;
    }

    _dyn_load_dependencies(mainDso);

    for (dso_t* dso = dsoList; dso != NULL; dso = dso->next)
    {
        if (!_dyn_relocate_dso(dso))
        {
            ioring_teardown(&ioring);
            return NULL;
        }
    }

    _dyn_run_all_inits();

    ioring_teardown(&ioring);
    return mainDso->entry;
}
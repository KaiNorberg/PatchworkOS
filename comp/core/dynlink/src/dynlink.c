#include "_libc/MAX_PATH.h"
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

#include <elf.h>
#include <libc/fs.h>
#include <libc/io.h>
#include <libc/math.h>
#include <stdarg.h>
#include <sys/auxv.h>

/**
 * @brief Dynamic Linker
 * @defgroup comp_dynlink Dynamic Linker
 * @ingroup comp
 *
 * @todo Write dynamic linker documentation.
 *
 * @see libc_elf for information on the ELF file format.
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

typedef enum
{
    DSO_NONE = 0,
    DSO_MAIN = (1 << 0),
    DSO_NODELETE = (1 << 1),
    DSO_INITIALIZED = (1 << 2),
    DSO_FINALIZED = (1 << 3),
    DSO_RELOCATED = (1 << 4),
} dso_flags_t;

#define DSO_MAX_DEPS 32

typedef struct dso
{
    struct dso* next;
    struct dso* prev;
    struct dso* free;
    void* base;
    void* entry;
    dso_flags_t flags;
    int refcount;
    Elf64_Addr loadOffset;
    Elf64_Xword loadSize;
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
    void (*fini)(void);
    void (**finiArray)(void);
    size_t finiArraySize;
    char name[MAX_PATH];
    struct dso* deps[DSO_MAX_DEPS];
    uint8_t depsAmount;
} dso_t;

#define MAX_DSOS 64
static dso_t dsos[MAX_DSOS];
static dso_t* dsoList = NULL;
static dso_t* dsoTail = NULL;
static dso_t* dsoFree = NULL;

#define MAX_ERROR 256
static char dsoError[MAX_ERROR];

static void _dyn_set_error(const char* string)
{
    size_t i = 0;
    while (string[i] != '\0' && i < MAX_ERROR - 1)
    {
        dsoError[i] = string[i];
        i++;
    }
    dsoError[i] = '\0';
}

static dso_t* _dyn_alloc_dso(void)
{
    if (dsoFree == NULL)
    {
        return NULL;
    }

    dso_t* dso = dsoFree;
    dsoFree = dso->free;

    unsigned char* p = (unsigned char*)dso;
    for (size_t i = 0; i < sizeof(dso_t); i++)
    {
        p[i] = 0;
    }

    return dso;
}

static void _dyn_free_dso(dso_t* dso)
{
    dso->free = dsoFree;
    dsoFree = dso;
}

static void _dyn_init_dso(dso_t* dso)
{
    if (dso->flags & DSO_INITIALIZED)
    {
        return;
    }

    for (uint8_t i = 0; i < dso->depsAmount; i++)
    {
        if (dso->deps[i])
        {
            _dyn_init_dso(dso->deps[i]);
        }
    }

    dso->flags |= DSO_INITIALIZED;

    if (dso->init != NULL)
    {
        dso->init();
    }
    if (dso->initArray != NULL)
    {
        size_t count = dso->initArraySize / sizeof(void*);
        for (size_t i = 0; i < count; i++)
        {
            if (dso->initArray[i])
            {
                dso->initArray[i]();
            }
        }
    }
}

static void _dyn_fini_dso(dso_t* dso)
{
    if (!(dso->flags & DSO_INITIALIZED) || (dso->flags & DSO_FINALIZED))
    {
        return;
    }

    dso->flags |= DSO_FINALIZED;

    if (dso->finiArray != NULL)
    {
        size_t count = dso->finiArraySize / sizeof(void*);
        for (size_t i = count; i > 0; i--)
        {
            if (dso->finiArray[i - 1])
            {
                dso->finiArray[i - 1]();
            }
        }
    }

    if (dso->fini != NULL)
    {
        dso->fini();
    }

    for (uint8_t i = 0; i < dso->depsAmount; i++)
    {
        if (dso->deps[i])
        {
            _dyn_fini_dso(dso->deps[i]);
        }
    }   
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

void* memset(void* s, int c, size_t n)
{
    unsigned char* p = (unsigned char*)s;
    while (n--)
    {
        *p++ = (unsigned char)c;
    }
    return s;
}

void* memcpy(void* dest, const void* src, size_t n)
{
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (n--)
    {
        *d++ = *s++;
    }
    return dest;
}

extern Elf64_Dyn _DYNAMIC[] HIDDEN;

static void _dyn_parse_auxv(void* stack, dyn_info_t* info)
{
    uintptr_t* p = (uintptr_t*)stack;
    uintptr_t argc = *p++;
    p += argc; // Skip argv pointers
    p++;       // Skip NULL terminator
    
    while (*p++); // Skip envp pointers and the NULL terminator

    auxv_t* auxv = (auxv_t*)p;
    for (size_t i = 0; auxv[i].a_type != AT_NULL; i++)
    {
        if (auxv[i].a_type == AT_BASE)
        {
            info->base = auxv[i].a_un.a_ptr;
        }
        else if (auxv[i].a_type == AT_EXECFD)
        {
            info->execFd = auxv[i].a_un.a_val;
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
            if (ELF64_R_TYPE(r->r_info) == R_X86_64_RELATIVE)
            {
                Elf64_Addr* target = (Elf64_Addr*)(info->base + r->r_offset);
                *target = (Elf64_Addr)info->base + r->r_addend;
            }
        }
    }
}

static void _dyn_itoa(char* buf, unsigned long val, unsigned int base)
{
    if (val == 0)
    {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    char tmp[32];
    int pos = 0;
    while (val > 0 && pos < 31)
    {
        unsigned long d = val % base;
        tmp[pos++] = (char)((d < 10) ? ('0' + d) : ('a' + d - 10));
        val /= base;
    }

    int i = 0;
    while (pos > 0)
    {
        buf[i++] = tmp[--pos];
    }
    buf[i] = '\0';
}

static int _dyn_vsnprintf(char* buf, size_t size, const char* fmt, va_list args)
{
    if (size == 0)
    {
        return 0;
    }

    char* out = buf;
    char* end = buf + size - 1;
    int written = 0;

    while (*fmt && out < end)
    {
        if (*fmt != '%')
        {
            *out++ = *fmt++;
            written++;
            continue;
        }

        fmt++;

        int isLong = 0;
        if (*fmt == 'l')
        {
            isLong = 1;
            fmt++;
        }

        switch (*fmt)
        {
        case 's':
        {
            const char* s = va_arg(args, const char*);
            if (s == NULL)
            {
                s = "(null)";
            }
            while (*s && out < end)
            {
                *out++ = *s++;
                written++;
            }
            break;
        }
        case 'Y':
        {
            unsigned long val = (unsigned long)va_arg(args, unsigned int);
            char hex[24];
            _dyn_itoa(hex, val, 16);
            if (out < end)
            {
                *out++ = '0';
                written++;
            }
            if (out < end)
            {
                *out++ = 'x';
                written++;
            }
            for (int i = 0; hex[i] && out < end; i++)
            {
                *out++ = hex[i];
                written++;
            }
            break;
        }
        case 'u':
        {
            unsigned long val = isLong ? va_arg(args, unsigned long) : (unsigned long)va_arg(args, unsigned int);
            char num[32];
            _dyn_itoa(num, val, 10);
            for (int i = 0; num[i] && out < end; i++)
            {
                *out++ = num[i];
                written++;
            }
            break;
        }
        case '%':
            if (out < end)
            {
                *out++ = '%';
                written++;
            }
            break;
        default:
            if (out < end)
            {
                *out++ = '%';
                written++;
            }
            if (out < end && *fmt)
            {
                *out++ = *fmt;
                written++;
            }
            break;
        }
        fmt++;
    }

    *out = '\0';
    return written;
}

static void _dyn_set_errorf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    _dyn_vsnprintf(dsoError, MAX_ERROR, fmt, args);
    va_end(args);
}

static int _dyn_printf(const char* fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    int len = _dyn_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    iowrite(FDOUT, IOBUF(buf, len), IOCUR, NULL);
    return len;
}

#define _DYN_ERRORF(fmt, ...) \
    do \
    { \
        _dyn_set_errorf(fmt, ##__VA_ARGS__); \
        _dyn_printf(fmt "\n", ##__VA_ARGS__); \
    } while (0)

static status_t _dyn_setup_ioring(void)
{
    _ioring = &ioring;
    return ioring_setup(&ioring, IORING_BASE, 16, 16);
}

static void _dyn_init_freelist(void)
{
    for (size_t i = 0; i < MAX_DSOS - 1; i++)
    {
        dsos[i].free = &dsos[i + 1];
    }
    dsos[MAX_DSOS - 1].free = NULL;
    dsoFree = &dsos[0];
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
            _DYN_ERRORF("dynlink: undefined symbol '%s' in '%s'", symName, dso->name);
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
                memcpy((void*)target, (const void*)symVal, sym->st_size);
            }
            else
            {
                _DYN_ERRORF("dynlink: failed to resolve COPY relocation for '%s'", symName);
                return false;
            }
            break;
        default:
            _DYN_ERRORF("dynlink: unknown relocation type %lu for '%s'", type, symName);
            return false;
        }
    }

    return true;
}

static bool _dyn_relocate_dso(dso_t* dso)
{
    if (dso->flags & DSO_RELOCATED)
    {
        return true;
    }

    dso->flags |= DSO_RELOCATED;

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

static void _dyn_parse_dynamic(dso_t* dso)
{
    if (dso->dynamic == NULL)
    {
        return;
    }

    for (size_t i = 0; dso->dynamic[i].d_tag != DT_NULL; i++)
    {
        Elf64_Dyn* d = &dso->dynamic[i];
        Elf64_Addr addr = d->d_un.d_ptr + dso->loadOffset;
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
        case DT_FINI:
            dso->fini = (void (*)(void))addr;
            break;
        case DT_FINI_ARRAY:
            dso->finiArray = (void (**)(void))addr;
            break;
        case DT_FINI_ARRAYSZ:
            dso->finiArraySize = d->d_un.d_val;
            break;
        default:
            break;
        }
    }
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
        _DYN_ERRORF("dynlink: failed to map ELF file '%s' %Y", name, status);
        return NULL;
    }

    size_t size = IOREG_LOAD(sizeReg);
    void* fileMap = (void*)IOREG_LOAD(fileMapReg);

    Elf64_File elf;
    if (elf64_validate(&elf, fileMap, size) != 0)
    {
        _DYN_ERRORF("dynlink: failed to validate ELF header for '%s'", name);
        iounmap(fileMap, size);
        return NULL;
    }

    Elf64_Addr minAddr = UINT64_MAX;
    Elf64_Addr maxAddr = 0;
    elf64_get_loadable_bounds(&elf, &minAddr, &maxAddr);

    if (minAddr > maxAddr)
    {
        _DYN_ERRORF("dynlink: invalid loadable bounds in '%s'", name);
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
        _DYN_ERRORF("dynlink: failed to map memory for '%s' %Y", name, status);
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
                _DYN_ERRORF("dynlink: failed to map segment from file '%s' %Y", name, status);
                iounmap(fileMap, size);
                return NULL;
            }

            if (phdr->p_memsz > phdr->p_filesz)
            {
                size_t zeroStart = vaddr + phdr->p_filesz;
                size_t zeroEnd = vaddrAligned + mapLen;
                if (zeroStart < zeroEnd)
                {
                    memset((void*)zeroStart, 0, zeroEnd - zeroStart);
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
            memcpy((void*)vaddr, (const void*)((uintptr_t)fileMap + phdr->p_offset), phdr->p_filesz);
        }

        size_t memszLen = ROUND_UP(phdr->p_memsz + diff, PAGE_SIZE);
        ioprotect((void*)vaddrAligned, memszLen, mapFlags);
    }

    dso_t* dso = _dyn_alloc_dso();
    if (!dso)
    {
        _DYN_ERRORF("dynlink: failed to allocate DSO structure for '%s'", name);
        iounmap(memMap, loadSize);
        iounmap(fileMap, size);
        return NULL;
    }

    _dyn_strcpy(dso->name, name);
    dso->base = memMap;
    dso->loadOffset = loadOffset;
    dso->loadSize = loadSize;
    dso->refcount = 1;
    dso->flags = isMain ? DSO_MAIN : 0;
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

    _dyn_parse_dynamic(dso);
    iounmap(fileMap, size);
    return dso;
}

static void _dyn_load_dependencies(dso_t* dso)
{
    if (dso->dynamic == NULL)
    {
        return;
    }

    for (size_t i = 0; dso->dynamic[i].d_tag != DT_NULL; i++)
    {
        Elf64_Dyn* d = &dso->dynamic[i];
        if (d->d_tag != DT_NEEDED)
        {
            continue;
        }

        const char* depName = dso->strtab + d->d_un.d_val;

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

        fd_t depFd = 0;
        status_t status = iowalk(FDCWD, FDROOT, path, &depFd);
        if (!IS_ERR(status))
        {
            dso_t* depDso = _dyn_load_elf(depFd, depName, false);
            iodrop(depFd);
            if (depDso != NULL)
            {
                depDso->prev = dsoTail;
                dsoTail->next = depDso;
                dsoTail = depDso;
                if (dso->depsAmount < DSO_MAX_DEPS)
                {
                    dso->deps[dso->depsAmount++] = depDso;
                }
                depDso->refcount++;
            }
            else
            {
                _dyn_printf("dynlink: failed to load dependency '%s'\n", path);
            }
        }
        else
        {
            _dyn_printf("dynlink: skipping dependency '%s'\n", path);
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
        _DYN_ERRORF("dynlink: failed to setup ioring %Y", status);
        return NULL;
    }

    _dyn_init_freelist();    

    dso_t* mainDso = _dyn_load_elf(info.execFd, "main", true);
    if (mainDso == NULL)
    {
        ioring_teardown(&ioring);
        return NULL;
    }

    dsoList = mainDso;
    dsoTail = mainDso;

    dso_t* linkerDso = _dyn_alloc_dso();
    if (linkerDso != NULL)
    {
        _dyn_strcpy(linkerDso->name, "dynlink.so");
        linkerDso->base = info.base;
        linkerDso->loadOffset = (Elf64_Addr)info.base;
        linkerDso->dynamic = _DYNAMIC;
        linkerDso->flags = DSO_INITIALIZED | DSO_RELOCATED | DSO_NODELETE;
        linkerDso->refcount = 1;
        _dyn_parse_dynamic(linkerDso);

        linkerDso->prev = dsoTail;
        dsoTail->next = linkerDso;
        dsoTail = linkerDso;
    }

    for (dso_t* dso = dsoList; dso != NULL; dso = dso->next)
    {
        _dyn_load_dependencies(dso);
    }

    for (dso_t* dso = dsoList; dso != NULL; dso = dso->next)
    {
        if (!_dyn_relocate_dso(dso))
        {
            ioring_teardown(&ioring);
            return NULL;
        }
    }

    for (dso_t* dso = dsoTail; dso != NULL; dso = dso->prev)
    {
        _dyn_init_dso(dso);
    }

    return mainDso->entry;
}

__attribute__((visibility("default"))) int dlclose(void* handle)
{
    dso_t* dso = (dso_t*)handle;
    if (dso == NULL || dso->refcount <= 0)
    {
        return -1;
    }

    dso->refcount--;
    if (dso->refcount == 0 && !(dso->flags & (DSO_MAIN | DSO_NODELETE)))
    {
        _dyn_fini_dso(dso);

        for (uint8_t i = 0; i < dso->depsAmount; i++)
        {
            dlclose(dso->deps[i]);
        }

        if (dso->prev != NULL)
        {
            dso->prev->next = dso->next;
        }
        if (dso->next != NULL)
        {
            dso->next->prev = dso->prev;
        }
        if (dsoList == dso)
        {
            dsoList = dso->next;
        }
        if (dsoTail == dso)
        {
            dsoTail = dso->prev;
        }

        iounmap(dso->base, dso->loadSize);
        _dyn_free_dso(dso);
    }

    return 0;
}

__attribute__((visibility("default"))) char* dlerror(void)
{
    return dsoError;
}

__attribute__((visibility("default"))) void* dlopen(const char* filename, int flag)
{
    if (filename == NULL)
    {
        return dsoList;
    }

    for (dso_t* dso = dsoList; dso != NULL; dso = dso->next)
    {
        if (_dyn_strcmp(dso->name, filename) == 0)
        {
            dso->refcount++;
            return dso;
        }
    }
    
    fd_t fd;
    status_t status = iowalk(FDCWD, FDROOT, filename, &fd);
    if (IS_ERR(status))
    {
        _dyn_set_errorf("dlopen: failed to open file '%s' %Y", filename, status);
        return NULL;
    }

    dso_t* dso = _dyn_load_elf(fd, filename, false);
    iodrop(fd);

    if (dso == NULL)
    {
        return NULL;
    }

    dso->prev = dsoTail;
    if (dsoTail != NULL)
    {
        dsoTail->next = dso;
    }
    dsoTail = dso;

    _dyn_load_dependencies(dso);

    for (dso_t* curr = dso; curr != NULL; curr = curr->next)
    {
        if (!_dyn_relocate_dso(curr))
        {
            return NULL;
        }
    }

    return dso;
}

__attribute__((visibility("default"))) void* dlsym(void* handle, const char* symbol)
{
    dso_t* start = (handle == NULL) ? dsoList : (dso_t*)handle;
    dso_t* foundDso = NULL;
    Elf64_Sym* sym = _dyn_lookup_symbol(symbol, start, &foundDso);

    if (sym != NULL && foundDso != NULL)
    {
        return (void*)(foundDso->loadOffset + sym->st_value);
    }

    _dyn_set_errorf("dlsym: symbol '%s' not found", symbol);
    return NULL;
}
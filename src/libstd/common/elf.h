#pragma once

#include <errno.h>
#include <stddef.h>
#include <sys/elf.h>

// Makes sure both the kernel and bootloader can use the elf functions
#ifdef _BOOT_
#include <efi.h>
#include <efilib.h>

#define elf_strcmp(Str1, Str2) strcmpa((void*)(Str1), (void*)(Str2))
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

#ifdef _KERNEL_
#include <kernel/log/log.h>
#endif
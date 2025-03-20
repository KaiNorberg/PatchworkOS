#include "loader.h"

#include "fs.h"
#include "vm.h"

#include <stdint.h>
#include <string.h>
#include <sys/elf.h>
#include <sys/math.h>

void loader_load_kernel(boot_kernel_t* kernel, CHAR16* path, EFI_HANDLE imageHandle)
{
    Print(L"Loading kernel...");

    EFI_FILE* file = fs_open(path, imageHandle);
    if (file == NULL)
    {
        Print(L"ERROR: Failed to load");

        while (1)
        {
            asm volatile("hlt");
        }
    }

    elf_hdr_t header;
    fs_read(file, sizeof(elf_hdr_t), &header);

    if (header.ident[0] != 0x7F || header.ident[1] != 'E' || header.ident[2] != 'L' || header.ident[3] != 'F')
    {
        Print(L"ERROR: File is corrupt");

        while (1)
        {
            asm volatile("hlt");
        }
    }

    uint64_t programHeaderTableSize = header.programHeaderAmount * header.programHeaderSize;
    elf_phdr_t* programHeaders = AllocatePool(programHeaderTableSize);
    fs_seek(file, header.programHeaderOffset);
    fs_read(file, programHeaderTableSize, programHeaders);

    uint64_t kernelStart = UINT64_MAX;
    uint64_t kernelEnd = 0;
    for (elf_phdr_t* programHeader = programHeaders; (uint64_t)programHeader < (uint64_t)programHeaders + programHeaderTableSize;
        programHeader = (elf_phdr_t*)((uint64_t)programHeader + header.programHeaderSize))
    {
        switch (programHeader->type)
        {
        case PT_LOAD:
        {
            kernelStart = MIN(kernelStart, programHeader->virtAddr);
            kernelEnd = MAX(kernelEnd, programHeader->virtAddr + programHeader->memorySize);
        }
        break;
        }
    }

    uint64_t kernelPageAmount = (kernelEnd - kernelStart) / EFI_PAGE_SIZE + 1;
    kernel->physStart = vm_alloc_pages((void*)kernelStart, kernelPageAmount, EFI_RESERVED);
    kernel->virtStart = (void*)kernelStart;
    kernel->entry = (void*)header.entry;
    kernel->length = kernelPageAmount * EFI_PAGE_SIZE;

    for (elf_phdr_t* programHeader = programHeaders; (uint64_t)programHeader < (uint64_t)programHeaders + programHeaderTableSize;
        programHeader = (elf_phdr_t*)((uint64_t)programHeader + header.programHeaderSize))
    {
        switch (programHeader->type)
        {
        case PT_LOAD:
        {
            fs_seek(file, programHeader->offset);

            memset((void*)programHeader->virtAddr, 0, programHeader->memorySize);
            fs_read(file, programHeader->fileSize, (void*)programHeader->virtAddr);
        }
        break;
        }
    }

    FreePool(programHeaders);
    fs_close(file);

    Print(L" done!\n");
}

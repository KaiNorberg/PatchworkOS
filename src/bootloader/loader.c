#include "loader.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/elf.h>

#include "fs.h"
#include "pml.h"
#include "vm.h"

void* load_kernel(CHAR16* path, EFI_HANDLE imageHandle)
{
    Print(L"Loading kernel...\n");

    EFI_FILE* file = fs_open(path, imageHandle);
    if (file == 0)
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
    elf_phdr_t* programHeaders = mem_alloc_pool(programHeaderTableSize, EfiLoaderData);
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
            if (kernelStart > programHeader->virtAddr)
            {
                kernelStart = programHeader->virtAddr;
            }
            if (kernelEnd < programHeader->virtAddr + programHeader->memorySize)
            {
                kernelEnd = programHeader->virtAddr + programHeader->memorySize;
            }
        }
        break;
        }
    }

    uint64_t kernelPageAmount = (kernelEnd - kernelStart) / EFI_PAGE_SIZE + 1;
    vm_alloc_kernel((void*)kernelStart, kernelPageAmount);

    for (elf_phdr_t* programHeader = programHeaders; (uint64_t)programHeader < (uint64_t)programHeaders + programHeaderTableSize;
         programHeader = (elf_phdr_t*)((uint64_t)programHeader + header.programHeaderSize))
    {
        switch (programHeader->type)
        {
        case PT_LOAD:
        {
            fs_seek(file, programHeader->offset);

            SetMem((void*)programHeader->virtAddr, programHeader->memorySize, 0);
            fs_read(file, programHeader->fileSize, (void*)programHeader->virtAddr);
        }
        break;
        }
    }

    mem_free_pool(programHeaders);
    fs_close(file);

    return (void*)header.entry;
}

void jump_to_kernel(void* entry, boot_info_t* bootInfo)
{
    void (*main)(boot_info_t*) = ((void (*)(boot_info_t*))entry);
    main(bootInfo);
}

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
        Print(L" ERROR: Failed to load");

        while (1)
        {
            asm volatile("hlt");
        }
    }

    elf_hdr_t header;
    fs_read(file, sizeof(elf_hdr_t), &header);

    if (!ELF_IS_VALID(&header))
    {
        Print(L" ERROR: File is corrupt");

        while (1)
        {
            asm volatile("hlt");
        }
    }

    uint64_t phdrTableSize = header.phdrAmount * header.phdrSize;
    elf_phdr_t* phdrs = AllocatePool(phdrTableSize);
    fs_seek(file, header.phdrOffset);
    fs_read(file, phdrTableSize, phdrs);

    uint64_t kernelStart = UINT64_MAX;
    uint64_t kernelEnd = 0;
    for (elf_phdr_t* phdr = phdrs; (uint64_t)phdr < (uint64_t)phdrs + phdrTableSize;
        phdr = (elf_phdr_t*)((uint64_t)phdr + header.phdrSize))
    {
        switch (phdr->type)
        {
        case ELF_PHDR_TYPE_LOAD:
        {
            kernelStart = MIN(kernelStart, phdr->virtAddr);
            kernelEnd = MAX(kernelEnd, phdr->virtAddr + phdr->memorySize);
        }
        break;
        }
    }

    uint64_t kernelPageAmount = (kernelEnd - kernelStart) / EFI_PAGE_SIZE + 1;

    Print(L" allocating %d KB... ", (kernelPageAmount * EFI_PAGE_SIZE) / 1000);
    kernel->physStart = vm_alloc_pages((void*)kernelStart, kernelPageAmount, EFI_RESERVED);
    kernel->virtStart = (void*)kernelStart;
    kernel->entry = (void*)header.entry;
    kernel->length = kernelPageAmount * EFI_PAGE_SIZE;

    for (elf_phdr_t* phdr = phdrs; (uint64_t)phdr < (uint64_t)phdrs + phdrTableSize;
        phdr = (elf_phdr_t*)((uint64_t)phdr + header.phdrSize))
    {
        switch (phdr->type)
        {
        case ELF_PHDR_TYPE_LOAD:
        {
            fs_seek(file, phdr->offset);

            memset((void*)phdr->virtAddr, 0, phdr->memorySize);
            fs_read(file, phdr->fileSize, (void*)phdr->virtAddr);
        }
        break;
        }
    }

    FreePool(phdrs);
    fs_close(file);

    Print(L" done!\n");
}

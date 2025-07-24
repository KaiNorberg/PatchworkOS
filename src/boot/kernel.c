#include "kernel.h"

#include "fs.h"
#include "mem.h"

#include <boot/boot_info.h>
#include <stdint.h>
#include <string.h>
#include <sys/elf.h>
#include <sys/math.h>
#include <sys/proc.h>

static BOOLEAN is_valid_phdr(const elf_phdr_t* phdr, uint64_t fileSize)
{
    if (phdr == NULL)
    {
        return FALSE;
    }

    if (phdr->offset > fileSize || phdr->fileSize > fileSize || phdr->offset + phdr->fileSize > fileSize)
    {
        return FALSE;
    }

    return TRUE;
}

static EFI_STATUS determine_kernel_bounds(const elf_phdr_t* phdrs, const elf_hdr_t* header, uint64_t phdrTableSize,
    uintptr_t* virtStart, uintptr_t* virtEnd)
{
    if (phdrs == NULL || header == NULL || virtStart == NULL || virtEnd == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    *virtStart = UINT64_MAX;
    *virtEnd = 0;
    BOOLEAN foundLoadable = FALSE;

    for (uint32_t i = 0; i < header->phdrAmount; i++)
    {
        const elf_phdr_t* phdr = (const elf_phdr_t*)((uint64_t)phdrs + (i * header->phdrSize));

        if ((uint64_t)phdr + sizeof(elf_phdr_t) > (uint64_t)phdrs + phdrTableSize)
        {
            return EFI_INVALID_PARAMETER;
        }

        if (phdr->type == ELF_PHDR_TYPE_LOAD)
        {
            foundLoadable = TRUE;
            *virtStart = MIN(*virtStart, phdr->virtAddr);
            *virtEnd = MAX(*virtEnd, phdr->virtAddr + phdr->memorySize);
        }
    }

    if (!foundLoadable)
    {
        return EFI_NOT_FOUND;
    }

    return EFI_SUCCESS;
}

static EFI_STATUS load_kernel_segments(EFI_FILE* file, uintptr_t physStart, uintptr_t virtStart, uint64_t kernelPageAmount,
    const elf_phdr_t* phdrs, const elf_hdr_t* header, uint64_t phdrTableSize, uint64_t fileSize)
{
    if (file == NULL || phdrs == NULL || header == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < header->phdrAmount; i++)
    {
        const elf_phdr_t* phdr = (const elf_phdr_t*)((uint64_t)phdrs + (i * header->phdrSize));

        if (phdr->type == ELF_PHDR_TYPE_LOAD)
        {
            if (!is_valid_phdr(phdr, fileSize))
            {
                return EFI_INVALID_PARAMETER;
            }

            EFI_STATUS status = fs_seek(file, phdr->offset);
            if (EFI_ERROR(status))
            {
                return status;
            }

            uintptr_t dest = physStart + (phdr->virtAddr - virtStart);
            if (dest < physStart || dest + phdr->memorySize > physStart + kernelPageAmount * PAGE_SIZE)
            {
                return EFI_INVALID_PARAMETER;
            }

            memset((void*)dest, 0, phdr->memorySize);

            if (phdr->fileSize > 0)
            {
                status = fs_read(file, phdr->fileSize, (void*)dest);
                if (EFI_ERROR(status))
                {
                    return status;
                }
            }
        }
    }

    return EFI_SUCCESS;
}

EFI_STATUS kernel_load(boot_kernel_t* kernel, EFI_HANDLE imageHandle)
{
    if (kernel == NULL || imageHandle == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    memset(kernel, 0, sizeof(boot_kernel_t));

    Print(L"Loading kernel... ");

    // Makes cleanup easier, even if i usually avoid declaring variables at the top of the function.
    EFI_FILE* root = NULL;
    EFI_FILE* kernelDir = NULL;
    EFI_FILE* file = NULL;
    elf_phdr_t* phdrs = NULL;
    EFI_STATUS status = EFI_SUCCESS;
    uint64_t kernelPageAmount = 0;

    status = fs_open_root_volume(&root, imageHandle);
    if (EFI_ERROR(status))
    {
        Print(L"failed to open root volume (0x%x)!\n", status);
        goto cleanup;
    }

    status = fs_open(&kernelDir, root, L"boot");
    if (EFI_ERROR(status))
    {
        Print(L"failed to open boot directory (0x%x)!\n", status);
        goto cleanup;
    }

    status = fs_open(&file, kernelDir, L"kernel");
    if (EFI_ERROR(status))
    {
        Print(L"failed to open kernel file (0x%x)!\n", status);
        goto cleanup;
    }

    EFI_FILE_INFO* fileInfo = LibFileInfo(file);
    if (fileInfo == NULL)
    {
        Print(L"failed to get kernel file info (0x%x)!\n", status);
        status = EFI_LOAD_ERROR;
        goto cleanup;
    }
    uint64_t fileSize = fileInfo->FileSize;
    FreePool(fileInfo);

    if (fileSize < sizeof(elf_hdr_t))
    {
        Print(L"kernel file too small (%llu bytes)!\n", fileSize);
        status = EFI_INVALID_PARAMETER;
        goto cleanup;
    }

    elf_hdr_t header;
    status = fs_read(file, sizeof(elf_hdr_t), &header);
    if (EFI_ERROR(status))
    {
        Print(L"failed to read ELF header (0x%x)!\n", status);
        goto cleanup;
    }

    if (!ELF_IS_VALID(&header))
    {
        Print(L"invalid ELF header in kernel file!\n");
        status = EFI_INVALID_PARAMETER;
        goto cleanup;
    }

    if (header.phdrAmount == 0)
    {
        Print(L"no program headers in kernel ELF!\n");
        status = EFI_INVALID_PARAMETER;
        goto cleanup;
    }

    if (header.phdrSize < sizeof(elf_phdr_t))
    {
        Print(L"invalid program header size (%u)!\n", header.phdrSize);
        status = EFI_INVALID_PARAMETER;
        goto cleanup;
    }

    uint64_t phdrTableSize = (uint64_t)header.phdrAmount * header.phdrSize;
    if (phdrTableSize > fileSize || header.phdrOffset > fileSize || header.phdrOffset + phdrTableSize > fileSize)
    {
        Print(L"program header table extends beyond file bounds!\n");
        status = EFI_INVALID_PARAMETER;
        goto cleanup;
    }

    phdrs = AllocatePool(phdrTableSize);
    if (phdrs == NULL)
    {
        Print(L"failed to allocate %llu bytes for program headers!\n", phdrTableSize);
        status = EFI_OUT_OF_RESOURCES;
        goto cleanup;
    }

    status = fs_seek(file, header.phdrOffset);
    if (EFI_ERROR(status))
    {
        Print(L"failed to seek to program header table (0x%x)!\n", status);
        goto cleanup;
    }

    status = fs_read(file, phdrTableSize, phdrs);
    if (EFI_ERROR(status))
    {
        Print(L"failed to read program header table (0x%x)!\n", status);
        goto cleanup;
    }

    uintptr_t virtStart = 0;
    uintptr_t virtEnd = 0;
    status = determine_kernel_bounds(phdrs, &header, phdrTableSize, &virtStart, &virtEnd);
    if (EFI_ERROR(status))
    {
        Print(L"failed to determine kernel bounds (0x%x)!\n", status);
        goto cleanup;
    }

    uint64_t kernelSize = virtEnd - virtStart;
    kernelPageAmount = BYTES_TO_PAGES(kernelSize);

    uintptr_t physStart;
    status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiReservedMemoryType, kernelPageAmount,
        &physStart);
    if (EFI_ERROR(status))
    {
        Print(L"failed to allocate %llu pages for kernel (0x%x)!\n", kernelPageAmount, status);
        goto cleanup;
    }

    kernel->virtStart = virtStart;
    kernel->physStart = physStart;
    kernel->entry = (void*)header.entry;
    kernel->size = kernelPageAmount * EFI_PAGE_SIZE;

    Print(L"phys=0x%llx virt=0x%llx size=%llu KB... ", kernel->physStart, kernel->virtStart, kernel->size / 1024);

    status = load_kernel_segments(file, (uintptr_t)kernel->physStart, kernel->virtStart, kernelPageAmount, phdrs, &header, phdrTableSize,
        fileSize);
    if (EFI_ERROR(status))
    {
        Print(L"failed to load kernel segments (0x%x)!\n", status);
        goto cleanup;
    }

    Print(L"done!\n");
    status = EFI_SUCCESS;

cleanup:
    if (kernel->physStart != 0 && EFI_ERROR(status))
    {
        uefi_call_wrapper(BS->FreePages, 3, kernel->physStart, kernelPageAmount);
        kernel->physStart = 0;
    }
    if (phdrs != NULL)
    {
        FreePool(phdrs);
    }
    if (file != NULL)
    {
        fs_close(file);
    }
    if (kernelDir != NULL)
    {
        fs_close(kernelDir);
    }
    if (root != NULL)
    {
        fs_close(root);
    }

    return status;
}

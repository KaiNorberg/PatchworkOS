#include "kernel.h"

#include <boot/boot_info.h>
#include <stdint.h>
#include <sys/elf.h>
#include <sys/math.h>
#include <sys/proc.h>

EFI_STATUS kernel_load(boot_kernel_t* kernel, EFI_FILE* rootHandle)
{
    if (kernel == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    Print(L"Loading kernel... ");

    EFI_FILE* kernelDir = NULL;
    EFI_FILE* file = NULL;
    EFI_STATUS status = EFI_SUCCESS;
    void* physStart = 0;

    status = uefi_call_wrapper(rootHandle->Open, 5, rootHandle, &kernelDir, L"kernel", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status))
    {
        Print(L"failed to open boot directory (0x%x)!\n", status);
        goto cleanup;
    }

    status = uefi_call_wrapper(kernelDir->Open, 5, kernelDir, &file, L"kernel", EFI_FILE_MODE_READ, 0);
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
    size_t fileSize = fileInfo->FileSize;
    FreePool(fileInfo);

    void* fileData = AllocatePool(fileSize);
    if (fileData == NULL)
    {
        Print(L"failed to allocate memory for kernel file (0x%x)!\n", status);
        status = EFI_OUT_OF_RESOURCES;
        goto cleanup;
    }

    size_t readSize = fileSize;
    status = uefi_call_wrapper(file->Read, 3, file, &readSize, fileData);
    if (EFI_ERROR(status) || readSize != fileSize)
    {
        Print(L"failed to read kernel file (0x%x)!\n", status);
        FreePool(fileData);
        goto cleanup;
    }

    uint64_t result = elf64_validate(&kernel->elf, fileData, fileSize);
    if (result != 0)
    {
        Print(L"invalid kernel ELF file %d!\n", result);
        FreePool(fileData);
        status = EFI_LOAD_ERROR;
        goto cleanup;
    }

    Elf64_Addr minVaddr = 0;
    Elf64_Addr maxVaddr = 0;
    elf64_get_loadable_bounds(&kernel->elf, &minVaddr, &maxVaddr);
    uint64_t kernelPageAmount = BYTES_TO_PAGES(maxVaddr - minVaddr);

    Print(L"allocating %llu pages... ", kernelPageAmount);
    status =
        uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiReservedMemoryType, kernelPageAmount, &physStart);
    if (EFI_ERROR(status))
    {
        Print(L"failed to allocate pages for kernel (0x%x)!\n", status);
        goto cleanup;
    }

    Print(L"loading segments to 0x%x... ", physStart);
    elf64_load_segments(&kernel->elf, (Elf64_Addr)physStart, minVaddr);
    kernel->physAddr = physStart;

    Print(L"done!\n");
    status = EFI_SUCCESS;

cleanup:
    if (EFI_ERROR(status) && physStart != NULL)
    {
        uefi_call_wrapper(BS->FreePages, 2, (EFI_PHYSICAL_ADDRESS)(uintptr_t)physStart,
            BYTES_TO_PAGES(maxVaddr - minVaddr));
        physStart = NULL;
    }
    if (file != NULL)
    {
        uefi_call_wrapper(file->Close, 1, file);
    }
    if (kernelDir != NULL)
    {
        uefi_call_wrapper(kernelDir->Close, 1, kernelDir);
    }

    return status;
}

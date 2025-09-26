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

static BOOLEAN is_valid_shdr(const elf_shdr_t* shdr, uint64_t fileSize)
{
    if (shdr == NULL)
    {
        return FALSE;
    }

    if (shdr->type != ELF_SHDR_TYPE_NOBITS)
    {
        if (shdr->offset > fileSize || shdr->size > fileSize || shdr->offset + shdr->size > fileSize)
        {
            return FALSE;
        }
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

static EFI_STATUS load_section_headers(EFI_FILE* file, boot_kernel_t* kernel, uint64_t fileSize)
{
    if (file == NULL || kernel == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    if (kernel->header.shdrAmount == 0 || kernel->header.shdrOffset == 0)
    {
        kernel->shdrs = NULL;
        kernel->shdrCount = 0;
        return EFI_SUCCESS;
    }

    uint64_t shdrTableSize = (uint64_t)kernel->header.shdrAmount * kernel->header.shdrSize;
    if (shdrTableSize > fileSize || kernel->header.shdrOffset > fileSize ||
        kernel->header.shdrOffset + shdrTableSize > fileSize)
    {
        Print(L"section header table extends beyond file bounds!\n");
        return EFI_INVALID_PARAMETER;
    }

    kernel->shdrs = AllocatePool(shdrTableSize);
    if (kernel->shdrs == NULL)
    {
        Print(L"failed to allocate %llu bytes for section headers!\n", shdrTableSize);
        return EFI_OUT_OF_RESOURCES;
    }

    EFI_STATUS status = fs_seek(file, kernel->header.shdrOffset);
    if (EFI_ERROR(status))
    {
        Print(L"failed to seek to section header table (0x%x)!\n", status);
        return status;
    }

    status = fs_read(file, shdrTableSize, kernel->shdrs);
    if (EFI_ERROR(status))
    {
        Print(L"failed to read section header table (0x%x)!\n", status);
        return status;
    }

    kernel->shdrCount = kernel->header.shdrAmount;
    return EFI_SUCCESS;
}

static elf_shdr_t* find_section_by_type(boot_kernel_t* kernel, elf_shdr_type_t sectionType)
{
    if (kernel == NULL || kernel->shdrs == NULL)
    {
        return NULL;
    }

    for (uint32_t i = 0; i < kernel->shdrCount; i++)
    {
        elf_shdr_t* shdr = (elf_shdr_t*)((uint64_t)kernel->shdrs + (i * kernel->header.shdrSize));
        if (shdr->type == sectionType)
        {
            return shdr;
        }
    }

    return NULL;
}

static EFI_STATUS load_symbol_table(EFI_FILE* file, boot_kernel_t* kernel, uint64_t fileSize)
{
    if (file == NULL || kernel == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    elf_shdr_t* symtabSection = find_section_by_type(kernel, ELF_SHDR_TYPE_SYMTAB);
    if (symtabSection == NULL)
    {
        symtabSection = find_section_by_type(kernel, ELF_SHDR_TYPE_DYNSYM);
        if (symtabSection == NULL)
        {
            kernel->symbols = NULL;
            kernel->symbolCount = 0;
            kernel->stringTable = NULL;
            kernel->stringTableSize = 0;
            return EFI_SUCCESS;
        }
    }

    if (!is_valid_shdr(symtabSection, fileSize))
    {
        Print(L"invalid symbol table section!\n");
        return EFI_INVALID_PARAMETER;
    }

    if (symtabSection->entrySize == 0 || symtabSection->entrySize < sizeof(elf_sym_t))
    {
        Print(L"invalid symbol table entry size (%llu)!\n", symtabSection->entrySize);
        return EFI_INVALID_PARAMETER;
    }

    uint32_t symbolCount = symtabSection->size / symtabSection->entrySize;
    if (symbolCount == 0)
    {
        kernel->symbols = NULL;
        kernel->symbolCount = 0;
        kernel->stringTable = NULL;
        kernel->stringTableSize = 0;
        return EFI_SUCCESS;
    }

    kernel->symbols = AllocatePool(symtabSection->size);
    if (kernel->symbols == NULL)
    {
        Print(L"failed to allocate %llu bytes for symbol table!\n", symtabSection->size);
        return EFI_OUT_OF_RESOURCES;
    }

    EFI_STATUS status = fs_seek(file, symtabSection->offset);
    if (EFI_ERROR(status))
    {
        Print(L"failed to seek to symbol table (0x%x)!\n", status);
        return status;
    }

    status = fs_read(file, symtabSection->size, kernel->symbols);
    if (EFI_ERROR(status))
    {
        Print(L"failed to read symbol table (0x%x)!\n", status);
        return status;
    }

    kernel->symbolCount = symbolCount;

    if (symtabSection->link < kernel->shdrCount)
    {
        elf_shdr_t* strtabSection =
            (elf_shdr_t*)((uint64_t)kernel->shdrs + (symtabSection->link * kernel->header.shdrSize));

        if (strtabSection->type == ELF_SHDR_TYPE_STRTAB && is_valid_shdr(strtabSection, fileSize))
        {
            kernel->stringTable = AllocatePool(strtabSection->size);
            if (kernel->stringTable == NULL)
            {
                Print(L"failed to allocate %llu bytes for string table!\n", strtabSection->size);
                return EFI_OUT_OF_RESOURCES;
            }

            status = fs_seek(file, strtabSection->offset);
            if (EFI_ERROR(status))
            {
                Print(L"failed to seek to string table (0x%x)!\n", status);
                return status;
            }

            status = fs_read(file, strtabSection->size, kernel->stringTable);
            if (EFI_ERROR(status))
            {
                Print(L"failed to read string table (0x%x)!\n", status);
                return status;
            }

            kernel->stringTableSize = strtabSection->size;
        }
    }

    if (kernel->stringTable == NULL)
    {
        kernel->stringTableSize = 0;
    }

    return EFI_SUCCESS;
}

static EFI_STATUS load_kernel_segments(EFI_FILE* file, uintptr_t physStart, uintptr_t virtStart,
    uint64_t kernelPageAmount, const elf_phdr_t* phdrs, const elf_hdr_t* header, uint64_t fileSize)
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

    status = fs_read(file, sizeof(elf_hdr_t), &kernel->header);
    if (EFI_ERROR(status))
    {
        Print(L"failed to read ELF header (0x%x)!\n", status);
        goto cleanup;
    }

    if (!ELF_IS_VALID(&kernel->header))
    {
        Print(L"invalid ELF header in kernel file!\n");
        status = EFI_INVALID_PARAMETER;
        goto cleanup;
    }

    if (kernel->header.phdrAmount == 0)
    {
        Print(L"no program headers in kernel ELF!\n");
        status = EFI_INVALID_PARAMETER;
        goto cleanup;
    }

    if (kernel->header.phdrSize < sizeof(elf_phdr_t))
    {
        Print(L"invalid program header size (%u)!\n", kernel->header.phdrSize);
        status = EFI_INVALID_PARAMETER;
        goto cleanup;
    }

    uint64_t phdrTableSize = (uint64_t)kernel->header.phdrAmount * kernel->header.phdrSize;
    if (phdrTableSize > fileSize || kernel->header.phdrOffset > fileSize ||
        kernel->header.phdrOffset + phdrTableSize > fileSize)
    {
        Print(L"program header table extends beyond file bounds!\n");
        status = EFI_INVALID_PARAMETER;
        goto cleanup;
    }

    kernel->phdrs = AllocatePool(phdrTableSize);
    if (kernel->phdrs == NULL)
    {
        Print(L"failed to allocate %llu bytes for program headers!\n", phdrTableSize);
        status = EFI_OUT_OF_RESOURCES;
        goto cleanup;
    }

    status = fs_seek(file, kernel->header.phdrOffset);
    if (EFI_ERROR(status))
    {
        Print(L"failed to seek to program header table (0x%x)!\n", status);
        goto cleanup;
    }

    status = fs_read(file, phdrTableSize, kernel->phdrs);
    if (EFI_ERROR(status))
    {
        Print(L"failed to read program header table (0x%x)!\n", status);
        goto cleanup;
    }

    Print(L"sections... ");
    status = load_section_headers(file, kernel, fileSize);
    if (EFI_ERROR(status))
    {
        Print(L"failed to load section headers (0x%x)!\n", status);
        goto cleanup;
    }

    Print(L"symbols... ");
    status = load_symbol_table(file, kernel, fileSize);
    if (EFI_ERROR(status))
    {
        Print(L"failed to load symbol table (0x%x)!\n", status);
        goto cleanup;
    }

    uintptr_t virtStart = 0;
    uintptr_t virtEnd = 0;
    status = determine_kernel_bounds(kernel->phdrs, &kernel->header, phdrTableSize, &virtStart, &virtEnd);
    if (EFI_ERROR(status))
    {
        Print(L"failed to determine kernel bounds (0x%x)!\n", status);
        goto cleanup;
    }

    uint64_t kernelSize = virtEnd - virtStart;
    kernelPageAmount = BYTES_TO_PAGES(kernelSize);

    uintptr_t physStart;
    status =
        uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiReservedMemoryType, kernelPageAmount, &physStart);
    if (EFI_ERROR(status))
    {
        Print(L"failed to allocate %llu pages for kernel (0x%x)!\n", kernelPageAmount, status);
        goto cleanup;
    }

    kernel->virtStart = virtStart;
    kernel->physStart = physStart;
    kernel->entry = (void*)kernel->header.entry;
    kernel->size = kernelPageAmount * EFI_PAGE_SIZE;

    Print(L"phys=0x%llx virt=0x%llx size=%llu KB... ", kernel->physStart, kernel->virtStart, kernel->size / 1024);

    status = load_kernel_segments(file, (uintptr_t)kernel->physStart, kernel->virtStart, kernelPageAmount,
        kernel->phdrs, &kernel->header, fileSize);
    if (EFI_ERROR(status))
    {
        Print(L"failed to load kernel segments (0x%x)!\n", status);
        goto cleanup;
    }

    if (kernel->symbols != NULL && kernel->symbolCount > 0)
    {
        Print(L"loaded %u symbols... ", kernel->symbolCount);
    }

    Print(L"done!\n");
    status = EFI_SUCCESS;

cleanup:
    if (status != EFI_SUCCESS)
    {
        if (kernel->phdrs != NULL)
        {
            FreePool(kernel->phdrs);
            kernel->phdrs = NULL;
        }
        if (kernel->shdrs != NULL)
        {
            FreePool(kernel->shdrs);
            kernel->shdrs = NULL;
        }
        if (kernel->symbols != NULL)
        {
            FreePool(kernel->symbols);
            kernel->symbols = NULL;
        }
        if (kernel->stringTable != NULL)
        {
            FreePool(kernel->stringTable);
            kernel->stringTable = NULL;
        }
    }

    if (kernel->physStart != 0 && EFI_ERROR(status))
    {
        uefi_call_wrapper(BS->FreePages, 3, kernel->physStart, kernelPageAmount);
        kernel->physStart = 0;
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

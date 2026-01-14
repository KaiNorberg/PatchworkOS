#include <boot/boot_info.h>
#include <efi.h>
#include <efilib.h>
#include <kernel/mem/paging.h>
#include <kernel/mem/paging_types.h>
#include <kernel/version.h>
#include <stddef.h>
#include <sys/defs.h>
#include <sys/elf.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/proc.h>

// Include functions directly to avoid multiple object files
#include <functions/assert/assert.c>
#include <functions/elf/elf64_get_loadable_bounds.c>
#include <functions/elf/elf64_load_segments.c>
#include <functions/elf/elf64_validate.c>

/**
 * @brief PatchworkOS UEFI Bootloader
 * @defgroup boot Bootloader
 *
 * @warning UEFI is exceptionally finicky as such to simpify the linking process which can have machine specific issues,
 * the bootloader should be compiled into a single object file.
 *
 * @{
 */

#define GOP_WIDTH 1920  ///< Ignored if `GOP_USE_DEFAULT_RES` is set to 1
#define GOP_HEIGHT 1080 ///< Ignored if `GOP_USE_DEFAULT_RES` is set to 1
#define GOP_USE_DEFAULT_RES 1

#define MEM_BASIC_ALLOCATOR_RESERVE_PERCENTAGE 10
#define MEM_BASIC_ALLOCATOR_MIN_PAGES 8192

#define EXIT_BOOT_SERVICES_MAX_RETRIES 5

/**
 * @brief Locates the ACPI RSDP from the EFI configuration table.
 *
 * @param systemTable Pointer to the EFI system table.
 * @return Pointer to the RSDP if found, `NULL` otherwise.
 */
static void* rsdp_locate(EFI_SYSTEM_TABLE* systemTable)
{
    if (systemTable == NULL || systemTable->ConfigurationTable == NULL)
    {
        return NULL;
    }

    EFI_GUID acpi2TableGuid = ACPI_20_TABLE_GUID;
    EFI_GUID acpi1TableGuid = ACPI_TABLE_GUID;

    void* rsdp = NULL;
    for (uint64_t i = 0; i < systemTable->NumberOfTableEntries; i++)
    {
        if (CompareGuid(&systemTable->ConfigurationTable[i].VendorGuid, &acpi2TableGuid) == 0)
        {
            if (CompareMem("RSD PTR ", systemTable->ConfigurationTable[i].VendorTable, 8) == 0)
            {
                rsdp = systemTable->ConfigurationTable[i].VendorTable;
                if (*((uint8_t*)rsdp + 15) < 2) // Check revision
                {
                    rsdp = NULL;
                    continue;
                }
            }
        }
    }

    if (rsdp != NULL)
    {
        Print(L"  ACPI 2.0+ RSDP found at 0x%lx\n", rsdp);
        return rsdp;
    }

    Print(L"  ACPI 2.0+ RSDP not found, falling back to ACPI 1.0\n");

    for (uint64_t i = 0; i < systemTable->NumberOfTableEntries; i++)
    {
        if (CompareGuid(&systemTable->ConfigurationTable[i].VendorGuid, &acpi1TableGuid) == 0)
        {
            if (CompareMem("RSD PTR ", systemTable->ConfigurationTable[i].VendorTable, 8) == 0)
            {
                rsdp = systemTable->ConfigurationTable[i].VendorTable;
            }
        }
    }

    if (rsdp != NULL)
    {
        Print(L"  ACPI 1.0 RSDP found at 0x%lx\n", rsdp);
        return rsdp;
    }

    Print(L"  WARNING: No ACPI RSDP found in configuration table\n");
    return NULL;
}

#if !(GOP_USE_DEFAULT_RES)

/**
 * @brief Finds the best matching graphics mode for the requested resolution.
 *
 * @param gop Pointer to the GOP protocol.
 * @param requestedWidth Desired horizontal resolution.
 * @param requestedHeight Desired vertical resolution.
 * @return The index of the best matching mode.
 */
static UINT32 gop_find_best_mode(EFI_GRAPHICS_OUTPUT_PROTOCOL* gop, int64_t requestedWidth, int64_t requestedHeight)
{
    UINT32 bestMode = 0;
    uint64_t bestDistance = UINT64_MAX;

    for (UINT32 modeIndex = 0; modeIndex < gop->Mode->MaxMode; modeIndex++)
    {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* modeInfo = NULL;
        UINTN infoSize = 0;

        EFI_STATUS status = uefi_call_wrapper(gop->QueryMode, 4, gop, modeIndex, &infoSize, &modeInfo);
        if (EFI_ERROR(status))
        {
            continue;
        }

        int64_t deltaX = (int64_t)modeInfo->HorizontalResolution - requestedWidth;
        int64_t deltaY = (int64_t)modeInfo->VerticalResolution - requestedHeight;
        uint64_t distance = (uint64_t)(deltaX * deltaX) + (uint64_t)(deltaY * deltaY);

        if (distance < bestDistance)
        {
            bestMode = modeIndex;
            bestDistance = distance;
        }

        if (distance == 0)
        {
            break;
        }
    }

    return bestMode;
}

/**
 * @brief Sets the graphics mode to best match the requested resolution.
 *
 * @param gop Pointer to the GOP protocol.
 * @param requestedWidth Desired horizontal resolution.
 * @param requestedHeight Desired vertical resolution.
 * @return On success, `EFI_SUCCESS`. On failure, an EFI error code.
 */
static EFI_STATUS gop_set_mode(EFI_GRAPHICS_OUTPUT_PROTOCOL* gop, int64_t requestedWidth, int64_t requestedHeight)
{
    UINT32 targetMode = gop_find_best_mode(gop, requestedWidth, requestedHeight);
    return uefi_call_wrapper(gop->SetMode, 2, gop, targetMode);
}

#endif

/**
 * @brief Initializes the GOP buffer structure with framebuffer information.
 *
 * Locates the Graphics Output Protocol, optionally sets the desired resolution,
 * and populates the `boot_gop_t` structure with framebuffer details.
 *
 * @param buffer Pointer to the `boot_gop_t` structure to populate.
 * @return On success, `EFI_SUCCESS`. On failure, an EFI error code.
 */
static EFI_STATUS gop_init(boot_gop_t* buffer)
{
    if (buffer == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

    EFI_STATUS status = uefi_call_wrapper(BS->LocateProtocol, 3, &gopGuid, NULL, (void**)&gop);
    if (EFI_ERROR(status))
    {
        Print(L"  ERROR: Failed to locate GOP (0x%lx)\n", status);
        return status;
    }

#if !(GOP_USE_DEFAULT_RES)
    Print(L"  Setting mode to %dx%d... ", GOP_WIDTH, GOP_HEIGHT);
    status = gop_set_mode(gop, GOP_WIDTH, GOP_HEIGHT);
    if (EFI_ERROR(status))
    {
        Print(L"  WARNING: Failed to set requested mode, using default (0x%lx)\n", status);
    }
#endif

    buffer->physAddr = (uint32_t*)gop->Mode->FrameBufferBase;
    buffer->virtAddr = (uint32_t*)PML_LOWER_TO_HIGHER(gop->Mode->FrameBufferBase);
    buffer->size = gop->Mode->FrameBufferSize;
    buffer->width = gop->Mode->Info->HorizontalResolution;
    buffer->height = gop->Mode->Info->VerticalResolution;
    buffer->stride = gop->Mode->Info->PixelsPerScanLine;

    switch (gop->Mode->Info->PixelFormat)
    {
    case PixelRedGreenBlueReserved8BitPerColor:
        Print(L"  Pixel format: RGB (32-bit)\n");
        break;
    case PixelBlueGreenRedReserved8BitPerColor:
        Print(L"  Pixel format: BGR (32-bit)\n");
        break;
    case PixelBitMask:
        Print(L"  WARNING: Pixel format is bitmask-based, may require custom handling\n");
        break;
    case PixelBltOnly:
        Print(L"  WARNING: Framebuffer not available, BLT only mode\n");
        break;
    default:
        Print(L"  WARNING: Unknown pixel format (%d)\n", gop->Mode->Info->PixelFormat);
        break;
    }

    Print(L"  GOP: 0x%lx-0x%lx %lux%lu, stride=%lu, size=%lu bytes\n", buffer->physAddr,
        (uintptr_t)buffer->physAddr + buffer->size, buffer->width, buffer->height, buffer->stride, buffer->size);

    return EFI_SUCCESS;
}

/**
 * @brief Basic page allocator state.
 *
 * This allocator is used after exiting boot services when UEFI memory
 * allocation is no longer available.
 */
static struct
{
    EFI_PHYSICAL_ADDRESS buffer;
    size_t maxPages;
    size_t pagesAllocated;
    boot_gop_t* gop;
    boot_memory_map_t* map;
} basicAllocator;

/**
 * @brief Panic error codes for debugging when boot services are unavailable.
 */
typedef enum
{
    PANIC_ALLOCATOR_EXHAUSTED = 0, // Red
    PANIC_PAGE_TABLE_INIT = 1,     // Green
    PANIC_IDENTITY_MAP = 2,        // Yellow
    PANIC_HIGHER_HALF_INVALID = 3, // Blue
    PANIC_HIGHER_HALF_MAP = 4,     // Magenta
    PANIC_KERNEL_MAP = 5,          // Cyan
    PANIC_GOP_MAP = 6,             // White
} panic_code_t;

/**
 * @brief Color values for panic display.
 */
static const uint32_t PANIC_COLORS[] = {
    0xFFFF0000, // Red
    0xFF00FF00, // Green
    0xFFFFFF00, // Yellow
    0xFF0000FF, // Blue
    0xFFFF00FF, // Magenta
    0xFF00FFFF, // Cyan
    0xFFFFFFFF, // White
};

/**
 * @brief Halts the system with a colored screen indicating the error.
 *
 * @param code The panic code indicating the type of error.
 */
_NORETURN static void panic_halt(panic_code_t code)
{
    if (basicAllocator.gop != NULL)
    {
        uint32_t color = PANIC_COLORS[code % ARRAY_SIZE(PANIC_COLORS)];
        boot_gop_t* gop = basicAllocator.gop;

        for (size_t y = 0; y < gop->height; y++)
        {
            for (size_t x = 0; x < gop->width; x++)
            {
                gop->physAddr[x + (y * gop->stride)] = color;
            }
        }
    }

    for (;;)
    {
        ASM("cli; hlt");
    }
}

/**
 * @brief Initializes the EFI memory map structure.
 *
 * @param map Pointer to the memory map structure to initialize.
 * @return On success, `EFI_SUCCESS`. On failure, an EFI error code.
 */
static EFI_STATUS mem_map_init(boot_memory_map_t* map)
{
    if (map == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    map->descriptors = LibMemoryMap(&map->length, &map->key, &map->descSize, &map->descVersion);
    if (map->descriptors == NULL)
    {
        return EFI_OUT_OF_RESOURCES;
    }

    for (size_t i = 0; i < map->length; i++)
    {
        EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, i);
        desc->VirtualStart = (EFI_VIRTUAL_ADDRESS)PML_LOWER_TO_HIGHER(desc->PhysicalStart);
    }

    return EFI_SUCCESS;
}

/**
 * @brief Free the memory map structure.
 *
 * @param map Pointer to the memory map structure to free.
 */
static void mem_map_cleanup(boot_memory_map_t* map)
{
    if (map != NULL && map->descriptors != NULL)
    {
        FreePool(map->descriptors);
        map->descriptors = NULL;
        map->length = 0;
    }
}

/**
 * @brief Calculates the total available conventional memory.
 *
 * @param map Pointer to the memory map.
 * @return Total number of available pages.
 */
static size_t mem_count_available_pages(boot_memory_map_t* map)
{
    size_t availablePages = 0;

    for (size_t i = 0; i < map->length; i++)
    {
        EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, i);
        if (desc->Type == EfiConventionalMemory)
        {
            availablePages += desc->NumberOfPages;
        }
    }

    return availablePages;
}

/**
 * @brief Initializes the basic page allocator.
 *
 * @return On success, `EFI_SUCCESS`. On failure, an EFI error code.
 */
static EFI_STATUS mem_allocator_init(void)
{
    boot_memory_map_t map = {0};
    EFI_STATUS status = mem_map_init(&map);
    if (EFI_ERROR(status))
    {
        Print(L"  ERROR: Failed to get memory map (0x%lx)\n", status);
        return status;
    }

    size_t availablePages = mem_count_available_pages(&map);
    size_t reservePages = (availablePages * MEM_BASIC_ALLOCATOR_RESERVE_PERCENTAGE) / 100;
    basicAllocator.maxPages = MAX(reservePages, MEM_BASIC_ALLOCATOR_MIN_PAGES);

    Print(L"  Reserving %lu pages...\n", basicAllocator.maxPages);

    status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, basicAllocator.maxPages,
        &basicAllocator.buffer);

    mem_map_cleanup(&map);

    if (EFI_ERROR(status))
    {
        Print(L"  ERROR: Failed to allocate page pool (0x%lx)\n", status);
        return status;
    }

    basicAllocator.pagesAllocated = 0;
    basicAllocator.gop = NULL;
    basicAllocator.map = NULL;

    Print(L"  Using %lu pages at 0x%lx\n", basicAllocator.maxPages, basicAllocator.buffer);

    return EFI_SUCCESS;
}

/**
 * @brief Allocate pages from the basic allocator.
 *
 * @param pages Output pointer to store the allocated pages.
 * @param amount Number of pages to allocate.
 * @return Always returns `0`.
 */
static uint64_t basic_allocator_alloc_pages(void** pages, size_t amount)
{
    if (basicAllocator.pagesAllocated + amount > basicAllocator.maxPages)
    {
        panic_halt(PANIC_ALLOCATOR_EXHAUSTED);
    }

    *pages = (void*)(uintptr_t)(basicAllocator.buffer + (basicAllocator.pagesAllocated * PAGE_SIZE));
    basicAllocator.pagesAllocated += amount;

    return 0;
}

/**
 * @brief Initializes the kernel page table with all required mappings.
 *
 * @param table Pointer to the page table structure.
 * @param map Pointer to the memory map.
 * @param gop Pointer to the GOP information.
 * @param kernel Pointer to the kernel information.
 */
static void mem_page_table_init(page_table_t* table, boot_memory_map_t* map, boot_gop_t* gop, boot_kernel_t* kernel)
{
    basicAllocator.gop = gop;
    basicAllocator.map = map;

    if (page_table_init(table, basic_allocator_alloc_pages, NULL) == ERR)
    {
        panic_halt(PANIC_PAGE_TABLE_INIT);
    }

    uintptr_t maxPhysicalAddress = 0;
    for (size_t i = 0; i < map->length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, i);
        uintptr_t regionEnd = desc->PhysicalStart + (desc->NumberOfPages * PAGE_SIZE);
        if (regionEnd > maxPhysicalAddress)
        {
            maxPhysicalAddress = regionEnd;
        }
    }

    if (page_table_map(table, 0, 0, BYTES_TO_PAGES(maxPhysicalAddress), PML_WRITE | PML_PRESENT, PML_CALLBACK_NONE) ==
        ERR)
    {
        panic_halt(PANIC_IDENTITY_MAP);
    }

    for (size_t i = 0; i < map->length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, i);

        if (desc->VirtualStart < PML_HIGHER_HALF_START)
        {
            panic_halt(PANIC_HIGHER_HALF_INVALID);
        }

        if (page_table_map(table, (void*)desc->VirtualStart, (void*)desc->PhysicalStart, desc->NumberOfPages,
                PML_WRITE | PML_PRESENT, PML_CALLBACK_NONE) == ERR)
        {
            panic_halt(PANIC_HIGHER_HALF_MAP);
        }
    }

    Elf64_Addr minVaddr = 0;
    Elf64_Addr maxVaddr = 0;
    elf64_get_loadable_bounds(&kernel->elf, &minVaddr, &maxVaddr);
    size_t kernelPageCount = BYTES_TO_PAGES(maxVaddr - minVaddr);

    if (page_table_map(table, (void*)minVaddr, kernel->physAddr, kernelPageCount, PML_WRITE | PML_PRESENT,
            PML_CALLBACK_NONE) == ERR)
    {
        panic_halt(PANIC_KERNEL_MAP);
    }

    if (page_table_map(table, (void*)gop->virtAddr, (void*)gop->physAddr, BYTES_TO_PAGES(gop->size),
            PML_WRITE | PML_PRESENT | PML_WRITE_THROUGH, PML_CALLBACK_NONE) == ERR)
    {
        panic_halt(PANIC_GOP_MAP);
    }
}

/**
 * @brief Copies a wide character string to a narrow character buffer.
 *
 * @param dest Destination buffer.
 * @param destSize Size of the destination buffer.
 * @param src Source wide string.
 * @return `true` if the string was fully copied, `false` if truncated.
 */
static bool wstr_to_str(char* dest, size_t destSize, const CHAR16* src)
{
    if (dest == NULL || destSize == 0)
    {
        return false;
    }

    size_t i;
    for (i = 0; i < destSize - 1 && src != NULL && src[i] != L'\0'; i++)
    {
        dest[i] = (char)src[i];
    }
    dest[i] = '\0';

    return src == NULL || src[i] == L'\0';
}

/**
 * @brief Frees a boot file structure and its data.
 *
 * @param file Pointer to the file structure to free.
 */
static void boot_file_free(boot_file_t* file)
{
    if (file != NULL)
    {
        if (file->data != NULL)
        {
            FreePool(file->data);
        }
        FreePool(file);
    }
}

/**
 * @brief Recursively frees a directory tree.
 *
 * @param dir Pointer to the directory structure to free.
 */
static void boot_dir_free(boot_dir_t* dir)
{
    if (dir == NULL)
    {
        return;
    }

    while (!list_is_empty(&dir->children))
    {
        list_entry_t* entry = list_pop_front(&dir->children);
        boot_dir_t* child = CONTAINER_OF(entry, boot_dir_t, entry);
        boot_dir_free(child);
    }

    while (!list_is_empty(&dir->files))
    {
        list_entry_t* entry = list_pop_front(&dir->files);
        boot_file_t* file = CONTAINER_OF(entry, boot_file_t, entry);
        boot_file_free(file);
    }

    FreePool(dir);
}

/**
 * @brief Loads a single file from an EFI file handle.
 *
 * @param parentDir The parent directory handle.
 * @param fileName The name of the file to load.
 * @return On success, pointer to the loaded file structure. On failure, `NULL`.
 */
static boot_file_t* disk_load_file(EFI_FILE* parentDir, const CHAR16* fileName)
{
    if (parentDir == NULL || fileName == NULL)
    {
        return NULL;
    }

    EFI_FILE* efiFile = NULL;
    boot_file_t* file = NULL;

    EFI_STATUS status = uefi_call_wrapper(parentDir->Open, 5, parentDir, &efiFile, (CHAR16*)fileName,
        EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM);
    if (EFI_ERROR(status))
    {
        return NULL;
    }

    file = AllocateZeroPool(sizeof(boot_file_t));
    if (file == NULL)
    {
        goto cleanup_file;
    }

    list_entry_init(&file->entry);
    wstr_to_str(file->name, MAX_NAME, fileName);

    EFI_FILE_INFO* fileInfo = LibFileInfo(efiFile);
    if (fileInfo == NULL)
    {
        goto cleanup_struct;
    }

    file->size = fileInfo->FileSize;
    FreePool(fileInfo);

    if (file->size > 0)
    {
        file->data = AllocatePool(file->size);
        if (file->data == NULL)
        {
            goto cleanup_struct;
        }

        UINTN readSize = file->size;
        status = uefi_call_wrapper(efiFile->Read, 3, efiFile, &readSize, file->data);
        if (EFI_ERROR(status) || readSize != file->size)
        {
            goto cleanup_struct;
        }
    }

    uefi_call_wrapper(efiFile->Close, 1, efiFile);
    return file;

cleanup_struct:
    boot_file_free(file);
cleanup_file:
    if (efiFile != NULL)
    {
        uefi_call_wrapper(efiFile->Close, 1, efiFile);
    }
    return NULL;
}

/**
 * @brief Recursively loads a directory and its contents.
 *
 * @param dirHandle The directory handle to read from.
 * @param dirName The name of the directory.
 * @return On success, pointer to the loaded directory structure. On failure, `NULL`.
 */
static boot_dir_t* disk_load_dir(EFI_FILE* dirHandle, const CHAR16* dirName)
{
    if (dirHandle == NULL || dirName == NULL)
    {
        return NULL;
    }

    boot_dir_t* dir = AllocateZeroPool(sizeof(boot_dir_t));
    if (dir == NULL)
    {
        return NULL;
    }

    list_entry_init(&dir->entry);
    wstr_to_str(dir->name, MAX_NAME, dirName);
    list_init(&dir->children);
    list_init(&dir->files);

    UINTN infoBufferSize = sizeof(EFI_FILE_INFO) + 256 * sizeof(CHAR16);
    EFI_FILE_INFO* fileInfo = AllocatePool(infoBufferSize);
    if (fileInfo == NULL)
    {
        boot_dir_free(dir);
        return NULL;
    }

    while (TRUE)
    {
        UINTN readSize = infoBufferSize;
        EFI_STATUS status = uefi_call_wrapper(dirHandle->Read, 3, dirHandle, &readSize, fileInfo);

        if (EFI_ERROR(status))
        {
            FreePool(fileInfo);
            boot_dir_free(dir);
            return NULL;
        }

        if (readSize == 0)
        {
            break;
        }

        if (fileInfo->FileName[0] == L'.' && fileInfo->FileName[1] == L'\0')
        {
            continue;
        }

        if (fileInfo->FileName[0] == L'.' && fileInfo->FileName[1] == L'.' && fileInfo->FileName[2] == L'\0')
        {
            continue;
        }

        if (fileInfo->Attribute & EFI_FILE_DIRECTORY)
        {
            EFI_FILE* childHandle = NULL;
            status = uefi_call_wrapper(dirHandle->Open, 5, dirHandle, &childHandle, fileInfo->FileName,
                EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM);

            if (EFI_ERROR(status))
            {
                FreePool(fileInfo);
                boot_dir_free(dir);
                return NULL;
            }

            boot_dir_t* child = disk_load_dir(childHandle, fileInfo->FileName);
            uefi_call_wrapper(childHandle->Close, 1, childHandle);

            if (child == NULL)
            {
                FreePool(fileInfo);
                boot_dir_free(dir);
                return NULL;
            }

            list_push_back(&dir->children, &child->entry);
        }
        else
        {
            boot_file_t* file = disk_load_file(dirHandle, fileInfo->FileName);
            if (file == NULL)
            {
                FreePool(fileInfo);
                boot_dir_free(dir);
                return NULL;
            }

            list_push_back(&dir->files, &file->entry);
        }
    }

    FreePool(fileInfo);
    return dir;
}

/**
 * @brief Loads the initial RAM disk from the boot volume.
 *
 * @param disk Pointer to the disk structure to populate.
 * @param rootHandle Handle to the root of the boot volume.
 * @return EFI_SUCCESS on success, error code otherwise.
 */
static EFI_STATUS disk_init(boot_disk_t* disk, EFI_FILE* rootHandle)
{
    if (disk == NULL || rootHandle == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    EFI_FILE* rootDir = NULL;
    EFI_STATUS status = uefi_call_wrapper(rootHandle->Open, 5, rootHandle, &rootDir, L"root", EFI_FILE_MODE_READ,
        EFI_FILE_READ_ONLY | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM);

    if (EFI_ERROR(status))
    {
        Print(L"  No 'root' subdirectory, loading from volume root...\n");
        disk->root = disk_load_dir(rootHandle, L"root");
    }
    else
    {
        disk->root = disk_load_dir(rootDir, L"root");
        uefi_call_wrapper(rootDir->Close, 1, rootDir);
    }

    if (disk->root == NULL)
    {
        Print(L"  ERROR: Failed to load root directory contents\n");
        return EFI_LOAD_ERROR;
    }

    return EFI_SUCCESS;
}

/**
 * @brief Loads and validates the kernel ELF file.
 *
 * @param kernel Pointer to the kernel structure to populate.
 * @param rootHandle Handle to the root of the boot volume.
 * @return On success, `EFI_SUCCESS`. On failure, an EFI error code.
 */
static EFI_STATUS kernel_load(boot_kernel_t* kernel, EFI_FILE* rootHandle)
{
    if (kernel == NULL || rootHandle == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    EFI_FILE* kernelDir = NULL;
    EFI_FILE* kernelFile = NULL;
    void* fileData = NULL;
    EFI_PHYSICAL_ADDRESS kernelPhys = 0;
    uint64_t kernelPageCount = 0;
    Elf64_Addr minVaddr = 0;
    Elf64_Addr maxVaddr = 0;
    EFI_STATUS status;

    status = uefi_call_wrapper(rootHandle->Open, 5, rootHandle, &kernelDir, L"kernel", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status))
    {
        Print(L"  ERROR: Failed to open kernel directory (0x%lx)\n", status);
        goto cleanup;
    }

    status = uefi_call_wrapper(kernelDir->Open, 5, kernelDir, &kernelFile, L"kernel", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status))
    {
        Print(L"  ERROR: Failed to open kernel file (0x%lx)\n", status);
        goto cleanup;
    }

    EFI_FILE_INFO* fileInfo = LibFileInfo(kernelFile);
    if (fileInfo == NULL)
    {
        Print(L"  ERROR: Failed to get kernel file info\n");
        status = EFI_LOAD_ERROR;
        goto cleanup;
    }

    size_t fileSize = fileInfo->FileSize;
    FreePool(fileInfo);

    if (fileSize == 0)
    {
        Print(L"  ERROR: Kernel file is empty\n");
        status = EFI_LOAD_ERROR;
        goto cleanup;
    }

    fileData = AllocatePool(fileSize);
    if (fileData == NULL)
    {
        Print(L"  ERROR: Failed to allocate memory for kernel file\n");
        status = EFI_OUT_OF_RESOURCES;
        goto cleanup;
    }

    UINTN readSize = fileSize;
    status = uefi_call_wrapper(kernelFile->Read, 3, kernelFile, &readSize, fileData);
    if (EFI_ERROR(status))
    {
        Print(L"  ERROR: Failed to read kernel file: (0x%lx)\n", status);
        goto cleanup;
    }

    if (readSize != fileSize)
    {
        Print(L"  ERROR: Incomplete kernel read (%lu of %lu bytes)\n", readSize, fileSize);
        status = EFI_LOAD_ERROR;
        goto cleanup;
    }

    Print(L"  File size %lu bytes\n", fileSize);

    uint64_t elfResult = elf64_validate(&kernel->elf, fileData, fileSize);
    if (elfResult != 0)
    {
        Print(L"  ERROR: Invalid kernel ELF (%lu)\n", elfResult);
        status = EFI_LOAD_ERROR;
        goto cleanup;
    }

    elf64_get_loadable_bounds(&kernel->elf, &minVaddr, &maxVaddr);
    size_t kernelSize = maxVaddr - minVaddr;
    kernelPageCount = BYTES_TO_PAGES(kernelSize);

    Print(L"  Allocating %lu pages for kernel...\n", kernelPageCount);
    status =
        uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiReservedMemoryType, kernelPageCount, &kernelPhys);
    if (EFI_ERROR(status))
    {
        Print(L"  ERROR: Failed to allocate kernel pages (0x%lx)", status);
        goto cleanup;
    }

    Print(L"  Loading kernel...\n");
    Elf64_Ehdr* header = (Elf64_Ehdr*)kernel->elf.header;
    for (uint32_t i = 0; i < header->e_phnum; i++)
    {
        Elf64_Phdr* phdr = ELF64_GET_PHDR(&kernel->elf, i);
        if (phdr->p_type != PT_LOAD)
        {
            Print(L"    Skipping non-loadable segment %u\n", i);
            continue;
        }
        Print(L"    Loading segment %u: vaddr=0x%lx, offset=0x%lx, filesz=%lu, memsz=%lu\n", i, phdr->p_vaddr,
            phdr->p_offset, phdr->p_filesz, phdr->p_memsz);
        void* dest = (void*)(kernelPhys + (phdr->p_vaddr - minVaddr));
        void* src = ELF64_AT_OFFSET(&kernel->elf, phdr->p_offset);
        Print(L"    First bytes: ");
        for (size_t j = 0; j < MIN(4, phdr->p_filesz); j++)
        {
            Print(L"%02x ", ((uint8_t*)src)[j]);
        }
        Print(L"\n");
        Print(L"    Copying %lu bytes from 0x%lx to 0x%lx\n", phdr->p_filesz, src, dest);
        memcpy(dest, src, phdr->p_filesz);
        if (phdr->p_memsz > phdr->p_filesz)
        {
            Print(L"    Zeroing %lu bytes at 0x%lx\n", phdr->p_memsz - phdr->p_filesz,
                (uintptr_t)dest + phdr->p_filesz);
            memset((void*)((uintptr_t)dest + phdr->p_filesz), 0, phdr->p_memsz - phdr->p_filesz);
        }
    }
    kernel->physAddr = (void*)kernelPhys;

    Print(L"  Entry Code: ");
    for (size_t i = 0; i < 16; i++)
    {
        Print(L"%02x ", ((uint8_t*)kernel->physAddr)[kernel->elf.header->e_entry - minVaddr + i]);
    }
    Print(L"\n");

    Print(L"  Kernel loaded at 0x%lx, entry=0x%lx, size=%lu\n", kernelPhys, kernel->elf.header->e_entry, kernelSize);

    status = EFI_SUCCESS;

cleanup:
    if (EFI_ERROR(status) && fileData != NULL)
    {
        FreePool(fileData);
    }
    if (kernelFile != NULL)
    {
        uefi_call_wrapper(kernelFile->Close, 1, kernelFile);
    }
    if (kernelDir != NULL)
    {
        uefi_call_wrapper(kernelDir->Close, 1, kernelDir);
    }
    if (EFI_ERROR(status) && kernelPhys != 0)
    {
        uefi_call_wrapper(BS->FreePages, 2, kernelPhys, kernelPageCount);
    }

    return status;
}

/**
 * @brief Displays the bootloader splash screen.
 */
static void splash_screen_display(void)
{
#ifdef NDEBUG
    Print(L"Start %a-bootloader %a (Built %a %a)\n", OS_NAME, OS_VERSION, __DATE__, __TIME__);
#else
    Print(L"Start %a-bootloader DEBUG %a (Built %a %a)\n", OS_NAME, OS_VERSION, __DATE__, __TIME__);
#endif
    Print(L"Copyright (C) 2026 Kai Norberg. MIT Licensed.\n");
}

/**
 * @brief Opens the root volume of the boot device.
 *
 * @param rootFile Output pointer to receive the root file handle.
 * @param imageHandle The loaded image handle.
 * @return On success, `EFI_SUCCESS`. On failure, an EFI error code.
 */
static EFI_STATUS volume_open_root(EFI_FILE** rootFile, EFI_HANDLE imageHandle)
{
    if (rootFile == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    EFI_LOADED_IMAGE* loadedImage = NULL;
    EFI_GUID lipGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_GUID fsGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_FILE_IO_INTERFACE* ioVolume = NULL;

    EFI_STATUS status = uefi_call_wrapper(BS->HandleProtocol, 3, imageHandle, &lipGuid, (void**)&loadedImage);
    if (EFI_ERROR(status))
    {
        Print(L"  ERROR: Failed to get loaded image protocol (0x%lx)\n", status);
        return status;
    }

    status = uefi_call_wrapper(BS->HandleProtocol, 3, loadedImage->DeviceHandle, &fsGuid, (void**)&ioVolume);
    if (EFI_ERROR(status))
    {
        Print(L"  ERROR: Failed to get file system protocol (0x%lx)\n", status);
        return status;
    }

    status = uefi_call_wrapper(ioVolume->OpenVolume, 2, ioVolume, rootFile);
    if (EFI_ERROR(status))
    {
        Print(L"  ERROR: Failed to open root volume (0x%lx)\n", status);
        return status;
    }

    return EFI_SUCCESS;
}

/**
 * @brief Populates the boot information structure.
 *
 * Initializes all boot data including GOP, RSDP, disk, and kernel.
 *
 * @param imageHandle The loaded image handle.
 * @param systemTable Pointer to the EFI system table.
 * @param bootInfo Pointer to the boot info structure to populate.
 * @return EFI_SUCCESS on success, error code otherwise.
 */
static EFI_STATUS boot_info_populate(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* systemTable, boot_info_t* bootInfo)
{
    if (bootInfo == NULL || systemTable == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    EFI_STATUS status;

    Print(L"Initializing GOP...\n");
    status = gop_init(&bootInfo->gop);
    if (EFI_ERROR(status))
    {
        Print(L"FATAL: Failed to initialize graphics (0x%lx)\n", status);
        return status;
    }

    Print(L"Locating ACPI RSDP...\n");
    bootInfo->rsdp = rsdp_locate(systemTable);
    if (bootInfo->rsdp == NULL)
    {
        Print(L"FATAL: ACPI RSDP not found in system configuration table\n");
        return EFI_NOT_FOUND;
    }

    bootInfo->runtimeServices = systemTable->RuntimeServices;

    Print(L"Loading boot volume...\n");
    EFI_FILE* rootHandle = NULL;
    status = volume_open_root(&rootHandle, imageHandle);
    if (EFI_ERROR(status))
    {
        Print(L"FATAL: Failed to open boot volume (0x%lx)\n", status);
        return status;
    }

    Print(L"Initializing disk...\n");
    status = disk_init(&bootInfo->disk, rootHandle);
    if (EFI_ERROR(status))
    {
        uefi_call_wrapper(rootHandle->Close, 1, rootHandle);
        return status;
    }

    Print(L"Loading kernel...\n");
    status = kernel_load(&bootInfo->kernel, rootHandle);
    if (EFI_ERROR(status))
    {
        uefi_call_wrapper(rootHandle->Close, 1, rootHandle);
        return status;
    }

    uefi_call_wrapper(rootHandle->Close, 1, rootHandle);

    Print(L"Boot info populated.\n");
    return EFI_SUCCESS;
}

/**
 * @brief Exits UEFI boot services and prepares for kernel handoff.
 *
 * @param imageHandle The loaded image handle.
 * @param bootInfo Pointer to the boot info structure.
 * @return On success, `EFI_SUCCESS`. On failure, an EFI error code.
 */
static EFI_STATUS boot_services_exit(EFI_HANDLE imageHandle, boot_info_t* bootInfo)
{
    if (bootInfo == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    EFI_STATUS status;
    UINT32 retryCount = 0;

    Print(L"Exiting boot services...\n");
    do
    {
        Print(L"  Attempt %d of %d...\n", retryCount + 1, EXIT_BOOT_SERVICES_MAX_RETRIES);
        if (retryCount > 0)
        {
            mem_map_cleanup(&bootInfo->memory.map);
        }

        status = mem_map_init(&bootInfo->memory.map);
        if (EFI_ERROR(status))
        {
            Print(L"  ERROR: Failed to get memory map: 0x%lx", status);
            return status;
        }

        status = uefi_call_wrapper(BS->ExitBootServices, 2, imageHandle, bootInfo->memory.map.key);

        if (status == EFI_SUCCESS)
        {
            break;
        }

        if (status == EFI_INVALID_PARAMETER)
        {
            Print(L"  WARNING: Stale key, retrying.\n");
            retryCount++;

            if (retryCount >= EXIT_BOOT_SERVICES_MAX_RETRIES)
            {
                Print(L"  ERROR: Maximum retries reached, aborting.\n");
                mem_map_cleanup(&bootInfo->memory.map);
                return EFI_ABORTED;
            }

            uefi_call_wrapper(BS->Stall, 1, 1000);
        }
        else
        {
            Print(L"  ERROR: Failed to exit boot services (0x%lx)\n", status);
            mem_map_cleanup(&bootInfo->memory.map);
            return status;
        }

    } while (retryCount < EXIT_BOOT_SERVICES_MAX_RETRIES);

    mem_page_table_init(&bootInfo->memory.table, &bootInfo->memory.map, &bootInfo->gop, &bootInfo->kernel);

    return EFI_SUCCESS;
}

/**
 * @brief UEFI entry point.
 *
 * @param imageHandle Handle to the loaded image.
 * @param systemTable Pointer to the EFI system table.
 * @return Will not return if successful. On failure, an EFI error code.
 */
EFI_STATUS efi_main(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* systemTable)
{
    InitializeLib(imageHandle, systemTable);

    uefi_call_wrapper(BS->SetWatchdogTimer, 4, 0, 0, 0, NULL);
    uefi_call_wrapper(systemTable->ConOut->ClearScreen, 1, systemTable->ConOut);

    splash_screen_display();

    Print(L"Initializing memory allocator...\n");
    EFI_STATUS status = mem_allocator_init();
    if (EFI_ERROR(status))
    {
        Print(L"FATAL: Failed to initialize memory allocator (0x%lx)\n", status);
        return status;
    }

    Print(L"Allocating boot info structure...\n");
    boot_info_t* bootInfo = AllocateZeroPool(sizeof(boot_info_t));
    if (bootInfo == NULL)
    {
        Print(L"FATAL: Failed to allocate boot info structure\n");
        return EFI_OUT_OF_RESOURCES;
    }

    status = boot_info_populate(imageHandle, systemTable, bootInfo);
    if (EFI_ERROR(status))
    {
        FreePool(bootInfo);
        return status;
    }

    status = boot_services_exit(imageHandle, bootInfo);
    if (EFI_ERROR(status))
    {
        return status;
    }

    cr3_write((uint64_t)bootInfo->memory.table.pml4);

    EFI_RUNTIME_SERVICES* rt = bootInfo->runtimeServices;
    uefi_call_wrapper(rt->SetVirtualAddressMap, 4, bootInfo->memory.map.length * bootInfo->memory.map.descSize,
        bootInfo->memory.map.descSize, bootInfo->memory.map.descVersion, bootInfo->memory.map.descriptors);

    void (*kernel_entry)(boot_info_t*) = (void (*)(boot_info_t*))bootInfo->kernel.elf.header->e_entry;
    kernel_entry(bootInfo);

    panic_halt(PANIC_ALLOCATOR_EXHAUSTED);
}

/** @} */
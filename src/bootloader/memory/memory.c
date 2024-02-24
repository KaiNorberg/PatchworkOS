#include "memory.h"

#include "common/boot_info/boot_info.h"
#include "efidef.h"
#include "efierr.h"
#include "efilib.h"
#include "x86_64/efibind.h"

void* memory_allocate_pages(uint64_t pageAmount, uint64_t memoryType)
{
	EFI_PHYSICAL_ADDRESS address = 0;
	EFI_STATUS status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, memoryType, pageAmount, &address);
    if (EFI_ERROR(status))
	{
		Print(L"ERROR: Unable to allocate pages!");

		while (1)
		{
			__asm__("HLT");
		}
	}

	return (void*)address;
}

void* memory_allocate_pool(uint64_t size, uint64_t memoryType)
{
	EFI_PHYSICAL_ADDRESS address = 0;
	EFI_STATUS status = uefi_call_wrapper(BS->AllocatePool, 4, memoryType, size, &address);
    if (EFI_ERROR(status))
	{
		Print(L"ERROR: Unable to allocate pages!");

		while (1)
		{
			__asm__("HLT");
		}
	}

	return (void*)address;
}

void memory_get_map(EfiMemoryMap* memoryMap)
{ 
	Print(L"Retrieving EFI Memory Map... ");

	memoryMap->base = LibMemoryMap(&memoryMap->descriptorAmount, &memoryMap->key, &memoryMap->descriptorSize, &memoryMap->descriptorVersion);

    Print(L"Done!\n\r");
}
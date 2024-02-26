#include "memory.h"

void* memory_allocate_pages(uint64_t pageAmount, uint64_t memoryType)
{
	EFI_PHYSICAL_ADDRESS address = 0;
	EFI_STATUS status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, memoryType, pageAmount, &address);
    if (EFI_ERROR(status))
	{
		Print(L"ERROR: Unable to allocate pages!");

		while (1)
		{
			asm volatile("hlt");
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
			asm volatile("hlt");
		}
	}

	return (void*)address;
}

void memory_free_pool(void* pool)
{
	uefi_call_wrapper(BS->FreePool, 1, pool);
}

void memory_map_populate(EfiMemoryMap* memoryMap)
{ 
	memoryMap->base = LibMemoryMap(&memoryMap->descriptorAmount, &memoryMap->key, &memoryMap->descriptorSize, &memoryMap->descriptorVersion);
}
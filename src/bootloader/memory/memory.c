#include "memory.h"

void* memory_allocate_pages(uint64_t pageAmount)
{
	EFI_PHYSICAL_ADDRESS address = 0;
	EFI_STATUS status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, pageAmount, &address);
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

EfiMemoryMap memory_get_map()
{ 
	Print(L"Retrieving EFI Memory Map... ");

    uint64_t descriptorAmount = 0;
    uint64_t mapKey = 0;
    uint64_t descriptorSize = 0;
    uint32_t descriptorVersion = 0;
	
	EFI_MEMORY_DESCRIPTOR* memoryMap = LibMemoryMap(&descriptorAmount, &mapKey, &descriptorSize, &descriptorVersion);
	
	EfiMemoryMap newMap;
	newMap.base = memoryMap;
	newMap.descriptorAmount = descriptorAmount;
	newMap.descriptorSize = descriptorSize;
	newMap.key = mapKey;

    Print(L"Done!\n\r");

	return newMap;
}
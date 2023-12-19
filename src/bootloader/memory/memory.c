#include "memory.h"

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
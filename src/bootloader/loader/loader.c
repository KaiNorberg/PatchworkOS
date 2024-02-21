#include "loader.h"

#include "memory/memory.h"
#include "file_system/file_system.h"
#include "page_directory/page_directory.h"
#include "string/string.h"

#include <common/common.h>

void jump_to_kernel(BootInfo* bootInfo, void* entry)
{
	void (*kernelMain)(BootInfo*) = ((void (*)(BootInfo*))entry);
	kernelMain(bootInfo);

	while (1)
	{
		asm volatile("HLT");
	}
}

void loader_load_kernel(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* systemTable, BootInfo* bootInfo)
{
    Print(L"Loading kernel... ");
	
	EFI_FILE* file = file_system_open(imageHandle, L"/boot/kernel.elf");
	if (file == NULL)
	{
		Print(L"ERROR: Failed to load");
				
		while (1)
		{
			__asm__("HLT");
		}
	}

	ElfHeader header;	
	file_system_read(file, sizeof(ElfHeader), &header);

    if(header.ident[0] != 0x7F ||
       header.ident[1] != 'E' ||
       header.ident[2] != 'L' ||
       header.ident[3] != 'F')
    {
		Print(L"ERROR: File is corrupt");
				
		while (1)
		{
			__asm__("HLT");
		}
    }

	uint64_t programHeaderTableSize = header.programHeaderAmount * header.programHeaderSize;
	ElfProgramHeader* programHeaders = AllocatePool(programHeaderTableSize);
	file_system_seek(file, header.programHeaderOffset);
	file_system_read(file, programHeaderTableSize, programHeaders);

	uint64_t kernelStart = 0;
	uint64_t kernelEnd = 0;
	for (ElfProgramHeader* programHeader = programHeaders; (uint64_t)programHeader < (uint64_t)programHeaders + programHeaderTableSize; programHeader = (ElfProgramHeader*)((uint64_t)programHeader + header.programHeaderSize))
	{
		switch (programHeader->type)
		{
		case PT_LOAD:
		{
			if (kernelStart == 0)
			{
				kernelStart = programHeader->virtualAddress;
			}
			kernelEnd = programHeader->virtualAddress + programHeader->memorySize;
		}
		break;
		}
	}
	uint64_t kernelPageAmount = (kernelEnd - kernelStart) / 0x1000 + 1;

	void* kernelBuffer = memory_allocate_pages(kernelPageAmount, EFI_MEMORY_TYPE_KERNEL);
	
	for (ElfProgramHeader* programHeader = programHeaders; (uint64_t)programHeader < (uint64_t)programHeaders + programHeaderTableSize; programHeader = (ElfProgramHeader*)((uint64_t)programHeader + header.programHeaderSize))
	{
		switch (programHeader->type)
		{
		case PT_LOAD:
		{
			file_system_seek(file, programHeader->offset);
			file_system_read(file, programHeader->fileSize, (void*)((uint64_t)kernelBuffer + (programHeader->virtualAddress - kernelStart)));
		}
		break;
		}
	}

	FreePool(programHeaders);
	file_system_close(file);
	Print(L"Done!\n\r");

	PageDirectory* kernelPageDirectory = page_directory_new();

	memory_get_map(&bootInfo->memoryMap);

	uint64_t totalPageAmount = 0;
    for (uint64_t i = 0; i < bootInfo->memoryMap.descriptorAmount; i++)
    {
        EfiMemoryDescriptor* desc = (EfiMemoryDescriptor*)((uint64_t)bootInfo->memoryMap.base + (i * bootInfo->memoryMap.descriptorSize));
		totalPageAmount += desc->NumberOfPages;
    }    
	
	page_directory_map_pages(kernelPageDirectory, 0, 0, totalPageAmount, PAGE_FLAG_WRITE);
    page_directory_map_pages(kernelPageDirectory, bootInfo->gopBuffer.base, bootInfo->gopBuffer.base, bootInfo->gopBuffer.size / 0x1000 + 1, PAGE_FLAG_WRITE);
	page_directory_map_pages(kernelPageDirectory, (void*)kernelStart, kernelBuffer, kernelPageAmount, PAGE_FLAG_WRITE);

	memory_get_map(&bootInfo->memoryMap);

    for (uint64_t i = 0; i < bootInfo->memoryMap.descriptorAmount; i++)
    {
        EfiMemoryDescriptor* desc = (EfiMemoryDescriptor*)((uint64_t)bootInfo->memoryMap.base + (i * bootInfo->memoryMap.descriptorSize));

		if (desc->Type == EFI_MEMORY_TYPE_KERNEL)
		{
			desc->VirtualStart = kernelStart;
		}
		else
		{
			desc->VirtualStart = desc->PhysicalStart;
		}
	}

	Print(L"Exiting boot services... ");
	systemTable->BootServices->ExitBootServices(imageHandle, bootInfo->memoryMap.key);
	Print(L"Done!\n\r");

	Print(L"Jumping to kernel... ");
	PAGE_DIRECTORY_LOAD(kernelPageDirectory);	
	jump_to_kernel(bootInfo, (void*)header.entry);
}
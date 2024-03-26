#include "loader.h"

#include <stddef.h>
#include <stdint.h>

#include <common/elf/elf.h>
#include <common/boot_info/boot_info.h>

#include "virtual_memory/virtual_memory.h"
#include "file_system/file_system.h"
#include "page_table/page_table.h"

void* load_kernel(CHAR16* path, EFI_HANDLE imageHandle)
{
    Print(L"Loading kernel...\n");

	EFI_FILE* file = file_system_open(path, imageHandle);
	if (file == 0)
	{
		Print(L"ERROR: Failed to load");
				
		while (1)
		{
			asm volatile("hlt");
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
			asm volatile("hlt");
		}
    }

	uint64_t programHeaderTableSize = header.programHeaderAmount * header.programHeaderSize;
	ElfProgramHeader* programHeaders = memory_allocate_pool(programHeaderTableSize, EfiLoaderData);
	file_system_seek(file, header.programHeaderOffset);
	file_system_read(file, programHeaderTableSize, programHeaders);
	
	uint64_t kernelStart = UINT64_MAX;
	uint64_t kernelEnd = 0;
	for (ElfProgramHeader* programHeader = programHeaders; 
		(uint64_t)programHeader < (uint64_t)programHeaders + programHeaderTableSize; 
		programHeader = (ElfProgramHeader*)((uint64_t)programHeader + header.programHeaderSize))
	{
		switch (programHeader->type)
		{
		case PT_LOAD:
		{
			if (kernelStart > programHeader->virtualAddress)
			{
				kernelStart = programHeader->virtualAddress;
			}
			if (kernelEnd < programHeader->virtualAddress + programHeader->memorySize)
			{
				kernelEnd = programHeader->virtualAddress + programHeader->memorySize;
			}
		}
		break;
		}
	}

	uint64_t kernelPageAmount = (kernelEnd - kernelStart) / EFI_PAGE_SIZE + 1;
	virtual_memory_allocate_kernel((void*)kernelStart, kernelPageAmount);
	
	for (ElfProgramHeader* programHeader = programHeaders; 
		(uint64_t)programHeader < (uint64_t)programHeaders + programHeaderTableSize; 
		programHeader = (ElfProgramHeader*)((uint64_t)programHeader + header.programHeaderSize))
	{
		switch (programHeader->type)
		{
		case PT_LOAD:
		{
			file_system_seek(file, programHeader->offset);
			
			SetMem((void*)programHeader->virtualAddress, programHeader->memorySize, 0);
			file_system_read(file, programHeader->fileSize, (void*)programHeader->virtualAddress);
		}
		break;
		}
	}

	memory_free_pool(programHeaders);
	file_system_close(file);

	return (void*)header.entry;
}

void jump_to_kernel(void* entry, BootInfo* bootInfo)
{
	void (*main)(BootInfo*) = ((void (*)(BootInfo*))entry);
	main(bootInfo);
}
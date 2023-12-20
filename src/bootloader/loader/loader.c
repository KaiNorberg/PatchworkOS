#include "loader.h"

#include "memory/memory.h"
#include "file_system/file_system.h"
#include "virtual_memory/virtual_memory.h"
#include "string/string.h"

#include "../common.h"

void loader_load_kernel(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* systemTable, BootInfo* bootInfo)
{
    Print(L"Loading kernel... ");
	
	VirtualAddressSpace* kernelAddressSpace = virtual_memory_create();

	EFI_FILE* file = file_system_open(imageHandle, L"/kernel/kernel.elf");
	if (file == NULL)
	{
		Print(L"ERROR: Failed to load");
				
		while (1)
		{
			__asm__("HLT");
		}
	}

	ElfHeader header;	
	uint64_t headerSize = sizeof(ElfHeader);
	file_system_read_to_buffer(file, &headerSize, &header);

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
	file_system_read_to_buffer(file, &programHeaderTableSize, programHeaders);

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

	void* kernelBuffer = 0;
	EFI_STATUS status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EFI_KERNEL_MEMORY_TYPE, kernelPageAmount, (void*)&kernelBuffer);
    if (EFI_ERROR(status))
	{
		Print(L"ERROR: Unable to allocate pages!");

		while (1)
		{
			__asm__("HLT");
		}
	}
	virtual_memory_remap_pages(kernelAddressSpace, (void*)kernelStart, kernelBuffer, kernelPageAmount);

	for (ElfProgramHeader* programHeader = programHeaders; (uint64_t)programHeader < (uint64_t)programHeaders + programHeaderTableSize; programHeader = (ElfProgramHeader*)((uint64_t)programHeader + header.programHeaderSize))
	{
		switch (programHeader->type)
		{
		case PT_LOAD:
		{
			uint64_t sizeOnFile = programHeader->fileSize;
			file_system_seek(file, programHeader->offset);
			file_system_read_to_buffer(file, &sizeOnFile, (void*)((uint64_t)kernelBuffer + (programHeader->virtualAddress - kernelStart)));
		}
		break;
		}
	}

	FreePool(programHeaders);
	file_system_close(file);
	Print(L"Done!\n\r");

	void (*kernelMain)(BootInfo*) = ((__attribute__((sysv_abi)) void (*)(BootInfo*))header.entry);

	EfiMemoryMap memoryMap = memory_get_map();

	uint64_t totalPageAmount = 0;
    for (uint64_t i = 0; i < memoryMap.descriptorAmount; i++)
    {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((uint64_t)memoryMap.base + (i * memoryMap.descriptorSize));
		totalPageAmount += desc->NumberOfPages;
    }
	for (uint64_t i = 0; i < totalPageAmount; i++)
	{
		virtual_memory_remap(kernelAddressSpace, (void*)(i * 0x1000), (void*)(i * 0x1000));
	}	
	for (uint64_t i = 0; i < bootInfo->screenbuffer->size / 0x1000 + 1; i++)
	{
		virtual_memory_remap(kernelAddressSpace, (void*)((uint64_t)bootInfo->screenbuffer->base + i * 0x1000), (void*)((uint64_t)bootInfo->screenbuffer->base + i * 0x1000));
	}

	memoryMap = memory_get_map();
	bootInfo->memoryMap = &memoryMap;

    for (uint64_t i = 0; i < memoryMap.descriptorAmount; i++)
    {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((uint64_t)memoryMap.base + (i * memoryMap.descriptorSize));

		if (desc->Type == EFI_KERNEL_MEMORY_TYPE)
		{
			desc->VirtualStart = kernelStart;
			break;
		}
	}

	Print(L"Exiting boot services... ");
	systemTable->BootServices->ExitBootServices(imageHandle, bootInfo->memoryMap->key);
	Print(L"Done\n\r");

	Print(L"Jumping to kernel...\n\r");
	VIRTUAL_MEMORY_LOAD_SPACE(kernelAddressSpace);
	kernelMain(bootInfo);
}
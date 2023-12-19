#include "loader.h"

#include "memory/memory.h"
#include "file_system/file_system.h"

void loader_load_kernel(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* systemTable, BootInfo* bootInfo)
{
    Print(L"Loading kernel... ");

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

	for (ElfProgramHeader* programHeader = programHeaders; (uint64_t)programHeader < (uint64_t)programHeaders + programHeaderTableSize; programHeader = (ElfProgramHeader*)((uint64_t)programHeader + header.programHeaderSize))
	{
		switch (programHeader->type)
		{
		case PT_LOAD:
		{
			int pages = (programHeader->memorySize * 0x1000 - 1) / 0x1000;
			uint64_t segment = programHeader->physicalAddress;
			systemTable->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, pages, &segment);

			uint64_t size = programHeader->fileSize;
			file_system_seek(file, programHeader->offset);
			file_system_read_to_buffer(file, &size, (void*)segment);
		}
		break;
		}
	}

	file_system_close(file);
	Print(L"Done!\n\r");

	void (*kernelMain)(BootInfo*) = ((__attribute__((sysv_abi)) void (*)(BootInfo*))header.entry);

	EfiMemoryMap memoryMap = memory_get_map();
	bootInfo->memoryMap = &memoryMap;

	Print(L"Exiting boot services... ");
	systemTable->BootServices->ExitBootServices(imageHandle, bootInfo->memoryMap->key);
	Print(L"Done\n\r");

	Print(L"Jumping to kernel...\n\r");
	kernelMain(bootInfo);
}
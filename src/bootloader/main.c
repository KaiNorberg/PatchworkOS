#include <efi.h>
#include <efilib.h>
#include <elf.h>

#define PSF_MAGIC 1078

typedef struct
{
	uint32_t* Base;
	uint64_t Size;
	uint32_t Width;
	uint32_t Height;
	uint32_t PixelsPerScanline;
} Framebuffer;

typedef struct
{
	uint16_t magic;
	uint8_t mode;
	uint8_t charsize;
} PSF_HEADER;

typedef struct
{
	PSF_HEADER* PSF_Header;
	void* GlyphBuffer;
} PSF_FONT;

typedef struct
{
	EFI_MEMORY_DESCRIPTOR* Base;
	uint64_t DescriptorAmount;
	uint64_t DescriptorSize;
	uint64_t Key;
} EFI_MEMORY_MAP;

EFI_HANDLE ImageHandle;
EFI_SYSTEM_TABLE* SystemTable;

const char* char16_to_char(CHAR16* string)
{
	uint64_t stringLength = StrLen(string);

	char* out = AllocatePool(stringLength + 1);

	for (uint64_t i = 0; i < stringLength; i++)
	{
		out[i] = string[i];
	}
	out[stringLength] = 0;

	return out;
}

int strcmp(const char* Str1, const char* Str2)
{
	int i = 0;
	while (Str1[i] != 0 && Str2[i] != 0)
	{
		if (Str1[i] != Str2[i])
		{
			return 0;
		}
		i++;
	}

	return (i != 0);
}

int memcmp(const void* aptr, const void* bptr, uint64_t n)
{
	const uint8_t* a = aptr, *b = bptr;
	for (uint64_t i = 0; i < n; i++)
	{
		if (a[i] < b[i]) return -1;
		else if (a[i] > b[i]) return 1;
	}
	return 0;
}

EFI_FILE* get_root_volume(EFI_HANDLE image)
{
	EFI_LOADED_IMAGE *loaded_image = NULL;
	EFI_GUID lipGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
	EFI_FILE_IO_INTERFACE *IOVolume;
	EFI_GUID fsGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
	EFI_FILE* Volume; 
	
	uefi_call_wrapper(BS->HandleProtocol, 3, image, &lipGuid, (void **) &loaded_image);

	uefi_call_wrapper(BS->HandleProtocol, 3, loaded_image->DeviceHandle, &fsGuid, (VOID*)&IOVolume);

	uefi_call_wrapper(IOVolume->OpenVolume, 2, IOVolume, &Volume);

	return Volume;
}

EFI_FILE* open_file(EFI_FILE* volume, CHAR16* path)
{
	EFI_FILE* fileHandle;
	
	/* open the file */
	uefi_call_wrapper(volume->Open, 5, volume, &fileHandle, path, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM);

	return fileHandle;
}

void seek(EFI_FILE* file, uint64_t offset)
{
	uefi_call_wrapper(file->SetPosition, 2, file, offset);
}

void* read_file(EFI_FILE* file, uint64_t readSize)
{
	uint8_t* buffer = AllocatePool(readSize);
	
	uefi_call_wrapper(file->Read, 3, file, &readSize, buffer);

	return buffer;
}

EFI_STATUS read_file_to_buffer(EFI_FILE* file, uint64_t* readSize, void* buffer)
{	
	return uefi_call_wrapper(file->Read, 3, file, readSize, buffer);
}

void close_file(EFI_FILE* file)
{
  	uefi_call_wrapper(file->Close, 1, file);
}

PSF_FONT load_psf_font(EFI_FILE* volume, CHAR16* path)
{		
	Print(L"Loading Font (%s)...\n\r", path);

	EFI_FILE* efiFile = open_file(volume, path);

	if (efiFile == NULL)
	{
		Print(L"ERROR: Failed to load font!\n\r");
		
		while (1)
		{
			__asm__("HLT");
		}
	}

	PSF_HEADER* fontHeader = read_file(efiFile, sizeof(PSF_HEADER));

	if (fontHeader->magic != PSF_MAGIC)
	{
		Print(L"ERROR: Invalid font magic found (%d)!\n\r", fontHeader->magic);
	}

	UINTN glyphBufferSize = fontHeader->charsize * 256;
	if (fontHeader->mode == 1)
	{
		glyphBufferSize = fontHeader->charsize * 512;
	}

	seek(efiFile, sizeof(PSF_HEADER));
	void* glyphBuffer = AllocatePool(glyphBufferSize);
	read_file_to_buffer(efiFile, &glyphBufferSize, glyphBuffer);

	PSF_FONT newFont;

	newFont.PSF_Header = fontHeader;
	newFont.GlyphBuffer = glyphBuffer;

	Print(L"FONT INFO\n\r");
	Print(L"Char Size: %d\n\r", newFont.PSF_Header->charsize);
	Print(L"Mode: 0x%x\n\r", newFont.PSF_Header->mode);
	Print(L"FONT INFO END\n\r");

	close_file(efiFile);

	return newFont;
}

Elf64_Ehdr load_elf_file(EFI_FILE* volume, CHAR16* path)
{	
	Print(L"Loading ELF (%s)...\n\r", path);

	EFI_FILE* efiFile = open_file(volume, path);
	if (efiFile == NULL)
	{
		Print(L"ERROR: Failed to load %s\n\r", path);
				
		while (1)
		{
			__asm__("HLT");
		}
	}

	Elf64_Ehdr header;	
	uint64_t headerSize = sizeof(Elf64_Ehdr);
	read_file_to_buffer(efiFile, &headerSize, &header);

	if (memcmp(&header.e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0 ||
		header.e_ident[EI_CLASS] != ELFCLASS64 ||
		header.e_ident[EI_DATA] != ELFDATA2LSB ||
		header.e_machine != EM_X86_64 ||
		header.e_version != EV_CURRENT)
	{
		Print(L"ERROR: %s is corrupted!", path);
	}

	seek(efiFile, header.e_phoff);
	UINTN size = header.e_phnum * header.e_phentsize;
	Elf64_Phdr* phdrs = read_file(efiFile, size);

	for (Elf64_Phdr* phdr = phdrs; (char*)phdr < (char*)phdrs + header.e_phnum * header.e_phentsize; phdr = (Elf64_Phdr*)((char*)phdr + header.e_phentsize))
	{
		switch (phdr->p_type)
		{
		case PT_LOAD:
		{
			int pages = (phdr->p_memsz * 0x1000 - 1) / 0x1000;
			Elf64_Addr segment = phdr->p_paddr;
			SystemTable->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, pages, &segment);

			seek(efiFile, phdr->p_offset);
			uint64_t size = phdr->p_filesz;
			read_file_to_buffer(efiFile, &size, (void*)segment);
		}
		break;
		}
	}

	close_file(efiFile);

	return header;
}

Framebuffer get_gop_framebuffer()
{	
	Print(L"Initializing GOP..\n\r");

	EFI_GUID GOP_GUID = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	EFI_GRAPHICS_OUTPUT_PROTOCOL* GOP;
	EFI_STATUS status = uefi_call_wrapper(BS->LocateProtocol, 3, &GOP_GUID, NULL, (void**)&GOP);

	if (EFI_ERROR(status))
	{
		Print(L"ERROR: GOP Failed!\n\r");
		
		while (1)
		{
			__asm__("HLT");
		}
	}

	Framebuffer NewBuffer;

	NewBuffer.Base = (uint32_t*)GOP->Mode->FrameBufferBase;
	NewBuffer.Size = GOP->Mode->FrameBufferSize;
	NewBuffer.Width = GOP->Mode->Info->HorizontalResolution;
	NewBuffer.Height = GOP->Mode->Info->VerticalResolution;
	NewBuffer.PixelsPerScanline = GOP->Mode->Info->PixelsPerScanLine;

	Print(L"GOP BUFFER INFO\n\r");
	Print(L"Base: 0x%x\n\r", NewBuffer.Base);
	Print(L"Size: 0x%x\n\r", NewBuffer.Size);
	Print(L"Width: %d\n\r", NewBuffer.Width);
	Print(L"Height: %d\n\r", NewBuffer.Height);
	Print(L"PixelsPerScanline: %d\n\r", NewBuffer.PixelsPerScanline);
	Print(L"GOP BUFFER INFO END\n\r");

	return NewBuffer;
}

EFI_MEMORY_MAP get_memory_map()
{ 
	Print(L"Retrieving EFI Memory Map...\n\r");

    uint64_t descriptorAmount = 0;
    uint64_t mapKey = 0;
    uint64_t descriptorSize = 0;
    uint32_t descriptorVersion = 0;
	
	EFI_MEMORY_DESCRIPTOR* memoryMap = LibMemoryMap(&descriptorAmount, &mapKey, &descriptorSize, &descriptorVersion);
	
	EFI_MEMORY_MAP newMap;
	newMap.Base = memoryMap;
	newMap.DescriptorAmount = descriptorAmount;
	newMap.DescriptorSize = descriptorSize;
	newMap.Key = mapKey;

	return newMap;
}

void* get_rsdp()
{	
	Print(L"Getting RSDP...\n\r");

	EFI_CONFIGURATION_TABLE* ConfigTable = SystemTable->ConfigurationTable;
	void* RSDP = 0;
	EFI_GUID Acpi2TableGuid = ACPI_20_TABLE_GUID;

	for (UINTN i = 0; i < SystemTable->NumberOfTableEntries; i++)
	{
		if (CompareGuid(&ConfigTable[i].VendorGuid, &Acpi2TableGuid) && strcmp("RSD PTR ", ConfigTable->VendorTable))
		{
			RSDP = ConfigTable->VendorTable;
		}
		ConfigTable++;
	}

	return RSDP;
}

typedef struct
{
	const char* Name;
	uint8_t* Data;
	uint64_t Size;
} File;

typedef struct Directory
{
	const char* Name;
	File* Files;
	uint64_t FileAmount;
	struct Directory* Directories;
	uint64_t DirectoryAmount;
} Directory;

UINT64 file_size(EFI_FILE* FileHandle)
{
	UINT64 ret;
	EFI_FILE_INFO *FileInfo;

	FileInfo = LibFileInfo(FileHandle);
	ret = FileInfo->FileSize;
	FreePool(FileInfo);
	return ret;
}

File create_file_struct(EFI_FILE* volume, CHAR16* path)
{	
	EFI_FILE* fileHandle = open_file(volume, path);

	UINT64 size = file_size(fileHandle);
	uint8_t* data = read_file(fileHandle, size);

	File output;
	output.Name = char16_to_char(path);
	output.Data = data;
	output.Size = size;

  	close_file(fileHandle);

	return output;
}

Directory create_directory_struct(EFI_FILE* volume, const char* name)
{		
	Directory out;
	out.Name = name;
	out.Files = 0;
	out.FileAmount = 0;
	out.Directories = 0;
	out.DirectoryAmount = 0;

	while (1) 
	{
		EFI_FILE_INFO* fileInfo;
		UINTN fileInfoSize = 0;

		EFI_STATUS status = read_file_to_buffer(volume, &fileInfoSize, NULL);
        if (status != EFI_BUFFER_TOO_SMALL) 
		{
            break;
		}

		fileInfo = AllocatePool(fileInfoSize);

		status = read_file_to_buffer(volume, &fileInfoSize, fileInfo);
		if (EFI_ERROR(status)) 
		{
			Print(L"Error reading file info\n");
			FreePool(fileInfo);
			break;
		}

		if (fileInfo->Attribute & EFI_FILE_DIRECTORY) 
		{
			if (StrCmp(fileInfo->FileName, L".") != 0 && StrCmp(fileInfo->FileName, L"..") != 0) 
			{
				EFI_FILE_PROTOCOL* subVolume = open_file(volume, fileInfo->FileName);

				Directory newDirectory = create_directory_struct(subVolume, char16_to_char(fileInfo->FileName));
				
				Directory* newDirectoryArray = AllocatePool(sizeof(Directory) * (out.DirectoryAmount + 1));
				if (out.DirectoryAmount != 0)
				{
					CopyMem(newDirectoryArray, out.Directories, sizeof(Directory) * out.DirectoryAmount);
					FreePool(out.Directories);
				}
				out.Directories = newDirectoryArray;
				out.Directories[out.DirectoryAmount] = newDirectory;
				out.DirectoryAmount++;

				close_file(subVolume);
			}
		}
		else
		{
			File newFile = create_file_struct(volume, fileInfo->FileName);
			
			File* newFileArray = AllocatePool(sizeof(File) * (out.FileAmount + 1));
			if (out.FileAmount != 0)
			{
				CopyMem(newFileArray, out.Files, sizeof(File) * out.FileAmount);
				FreePool(out.Files);
			}
			out.Files = newFileArray;
			out.Files[out.FileAmount] = newFile;
			out.FileAmount++;
			
		}

		FreePool(fileInfo);
	}

	return out;
}

typedef struct
{
	Framebuffer* Screenbuffer;
	PSF_FONT* Font;	
	EFI_MEMORY_MAP* MemoryMap;
	void* RSDP;
	EFI_RUNTIME_SERVICES *RT;
	Directory* RootDirectory;
} BootInfo;

EFI_STATUS efi_main(EFI_HANDLE In_ImageHandle, EFI_SYSTEM_TABLE* In_SystemTable)
{
	ImageHandle = In_ImageHandle;
	SystemTable = In_SystemTable;

	InitializeLib(ImageHandle, SystemTable);

	Print(L"BootLoader loaded!\n\r");

	EFI_FILE* rootVolume = get_root_volume(In_ImageHandle);

	Print(L"Caching file system..\n\r");
	Directory rootDirectory = create_directory_struct(rootVolume, "ROOT");
	
	void* rsdp = get_rsdp();
	Framebuffer screenbuffer = get_gop_framebuffer();

	EFI_FILE* kernelVolume = open_file(rootVolume, L"KERNEL");
	EFI_FILE* fontsVolume = open_file(rootVolume, L"FONTS");

	Elf64_Ehdr kernelFile = load_elf_file(kernelVolume, L"Kernel.elf");

	PSF_FONT ttyFont = load_psf_font(fontsVolume, L"zap-vga16.psf");

	close_file(kernelVolume);
	close_file(fontsVolume);

	EFI_MEMORY_MAP memoryMap = get_memory_map();

	BootInfo bootInfo;
	bootInfo.Screenbuffer = &screenbuffer;
	bootInfo.Font = &ttyFont;
	bootInfo.MemoryMap = &memoryMap;
	bootInfo.RSDP = rsdp;
	bootInfo.RT = SystemTable->RuntimeServices;
	bootInfo.RootDirectory = &rootDirectory;

	void (*KernelMain)(BootInfo*) = ((__attribute__((sysv_abi)) void (*)(BootInfo*)) kernelFile.e_entry);

	Print(L"Exiting boot services...\n\r");
	SystemTable->BootServices->ExitBootServices(ImageHandle, memoryMap.Key);

	Print(L"Entering Kernel...\n\r");
	KernelMain(&bootInfo);

	return EFI_SUCCESS; // Exit the UEFI application
}

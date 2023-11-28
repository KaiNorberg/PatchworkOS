#include <efi.h>
#include <efilib.h>
#include <elf.h>

#define PSF_MAGIC0 0x36
#define PSF_MAGIC1 0x04

typedef unsigned long long size_t;

typedef struct
{
	unsigned int* Base;
	size_t Size;
	unsigned int Width;
	unsigned int Height;
	unsigned int PixelsPerScanline;
} Framebuffer;

typedef struct
{
	unsigned char magic[2];
	unsigned char mode;
	unsigned char charsize;
} PSF_HEADER;

typedef struct
{
	PSF_HEADER* PSF_Header;
	void* glyphBuffer;
} PSF_FONT;

typedef struct
{
	EFI_MEMORY_DESCRIPTOR* Base;
	uint64_t Size;
	uint64_t DescSize;
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

int memcmp(const void* aptr, const void* bptr, size_t n)
{
	const unsigned char* a = aptr, *b = bptr;
	for (size_t i = 0; i < n; i++)
	{
		if (a[i] < b[i]) return -1;
		else if (a[i] > b[i]) return 1;
	}
	return 0;
}

EFI_FILE* load_efi_file(EFI_FILE* Directory, CHAR16* Path)
{
	Print(L"Loading File (%s)...\n\r", Path);

	EFI_FILE* LoadedFile;

	EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
	SystemTable->BootServices->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (void**)&LoadedImage);

	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* FileSystem;
	SystemTable->BootServices->HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void**)&FileSystem);

	if (Directory == NULL)
	{
		FileSystem->OpenVolume(FileSystem, &Directory);
	}

	EFI_STATUS s = Directory->Open(Directory, &LoadedFile, Path, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY);

	if (s != EFI_SUCCESS)
	{
		return NULL;
	}

	return LoadedFile;
}

PSF_FONT load_psf_font(EFI_FILE* Directory, CHAR16* Path)
{
	EFI_FILE* efiFile = load_efi_file(Directory, Path);

	if (efiFile == NULL)
	{
		Print(L"ERROR: Failed to load font!\n\r");
		
		while (1)
		{
			__asm__("HLT");
		}
	}

	PSF_HEADER* FontHeader;
	SystemTable->BootServices->AllocatePool(EfiLoaderData, sizeof(PSF_HEADER), (void**)&FontHeader);
	UINTN size = sizeof(PSF_HEADER);
	efiFile->Read(efiFile, &size, FontHeader);

	/*if (FontHeader->magic[0] != PSF_MAGIC0 || FontHeader->magic[1] != PSF_MAGIC1)
	{
		Print(L"ERROR: Invalid font loaded!\n\r");
		
		while (1)
		{
			__asm__("HLT");
		}
	}*/

	UINTN GlyphBufferSize = FontHeader->charsize * 256;
	if (FontHeader->mode == 1)
	{
		GlyphBufferSize = FontHeader->charsize * 512;
	}

	void* GlyphBuffer;
	efiFile->SetPosition(efiFile, sizeof(PSF_HEADER));
	SystemTable->BootServices->AllocatePool(EfiLoaderData, GlyphBufferSize, (void**)&GlyphBuffer);
	efiFile->Read(efiFile, &GlyphBufferSize, GlyphBuffer);

	PSF_FONT newFont;

	newFont.PSF_Header = FontHeader;
	newFont.glyphBuffer = (char*)GlyphBuffer;

	Print(L"FONT INFO\n\r");
	Print(L"Char Size: %d\n\r", newFont.PSF_Header->charsize);
	Print(L"Mode: 0x%x\n\r", newFont.PSF_Header->mode);
	Print(L"FONT INFO END\n\r");

	efiFile->Close(efiFile);

	return newFont;
}

Elf64_Ehdr load_elf_file(EFI_FILE* Directory, CHAR16* Path)
{
	EFI_FILE* efiFile = load_efi_file(Directory, Path);
	if (efiFile == NULL)
	{
		Print(L"ERROR: Failed to load %s\n\r", Path);
				
		while (1)
		{
			__asm__("HLT");
		}
	}

	Print(L"Reading ELF File...\n\r");

	Elf64_Ehdr Header;
	UINTN FileInfoSize;
	EFI_FILE_INFO* FileInfo;
	efiFile->GetInfo(efiFile, &gEfiFileInfoGuid, &FileInfoSize, NULL);
	SystemTable->BootServices->AllocatePool(EfiLoaderData, FileInfoSize, (void**)&FileInfo);
	efiFile->GetInfo(efiFile, &gEfiFileInfoGuid, &FileInfoSize, (void**)&FileInfo);
	UINTN Size = sizeof(Header);
	efiFile->Read(efiFile, &Size, &Header);

	if (memcmp(&Header.e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0 ||
		Header.e_ident[EI_CLASS] != ELFCLASS64 ||
		Header.e_ident[EI_DATA] != ELFDATA2LSB ||
		Header.e_machine != EM_X86_64 ||
		Header.e_version != EV_CURRENT)
	{
		Print(L"ERROR: %s is corrupted!", Path);
	}

	Elf64_Phdr* phdrs;
	efiFile->SetPosition(efiFile, Header.e_phoff);
	UINTN size = Header.e_phnum * Header.e_phentsize;
	SystemTable->BootServices->AllocatePool(EfiLoaderData, size, (void**)&phdrs);
	efiFile->Read(efiFile, &size, phdrs);

	for (Elf64_Phdr* phdr = phdrs; (char*)phdr < (char*)phdrs + Header.e_phnum * Header.e_phentsize; phdr = (Elf64_Phdr*)((char*)phdr + Header.e_phentsize))
	{
		switch (phdr->p_type)
		{
		case PT_LOAD:
		{
			int pages = (phdr->p_memsz * 0x1000 - 1) / 0x1000;
			Elf64_Addr segment = phdr->p_paddr;
			SystemTable->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, pages, &segment);

			efiFile->SetPosition(efiFile, phdr->p_offset);
			UINTN size = phdr->p_filesz;
			efiFile->Read(efiFile, &size, (void*)segment);
		}
		break;
		}
	}

	efiFile->Close(efiFile);

	return Header;
}

Framebuffer get_gop_framebuffer()
{
	EFI_GUID GOP_GUID = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	EFI_GRAPHICS_OUTPUT_PROTOCOL* GOP;
	EFI_STATUS status = uefi_call_wrapper(BS->LocateProtocol, 3, &GOP_GUID, NULL, (void**)&GOP);

	Print(L"Initializing GOP..\n\r");
	if (EFI_ERROR(status))
	{
		Print(L"ERROR: GOP Failed!\n\r");
		
		while (1)
		{
			__asm__("HLT");
		}
	}

	Framebuffer NewBuffer;

	NewBuffer.Base = (unsigned int*)GOP->Mode->FrameBufferBase;
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

	EFI_MEMORY_DESCRIPTOR* Base = NULL;
	UINTN MapSize, MapKey;
	UINTN DescriptorSize;
	UINT32 DescriptorVersion;
	SystemTable->BootServices->GetMemoryMap(&MapSize, Base, &MapKey, &DescriptorSize, &DescriptorVersion);
	SystemTable->BootServices->AllocatePool(EfiLoaderData, MapSize, (void**)&Base);
	SystemTable->BootServices->GetMemoryMap(&MapSize, Base, &MapKey, &DescriptorSize, &DescriptorVersion);

	EFI_MEMORY_MAP NewMap;
	NewMap.Base = Base;
	NewMap.Size = MapSize;
	NewMap.DescSize = DescriptorSize;
	NewMap.Key = MapKey;

	return NewMap;
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

EFI_FILE_INFO* get_file_info(EFI_FILE* file) 
{
    EFI_STATUS status;
    EFI_FILE_INFO* fileInfo;
    UINTN bufferSize;

    bufferSize = 0;
    status = file->GetInfo(file, &gEfiFileInfoGuid, &bufferSize, NULL);

    if (status == EFI_BUFFER_TOO_SMALL) 
	{
        status = SystemTable->BootServices->AllocatePool(EfiLoaderData, bufferSize, (void**)&fileInfo);
        if (EFI_ERROR(status)) 
		{
            Print(L"Error allocating memory for file info\n");
            return 0;
        }

        status = file->GetInfo(file, &gEfiFileInfoGuid, &bufferSize, fileInfo);

        if (EFI_ERROR(status)) 
		{
            Print(L"Error getting file info\n");
            SystemTable->BootServices->FreePool(fileInfo);
            return 0;
        }

		return fileInfo;
    } 
	else 
	{
        Print(L"Error getting file info buffer size\n");
		return 0;
	}
}

Directory create_directory(const char* name, uint64_t fileAmount, uint64_t directoryAmount)
{
	Directory directory;
	directory.Name = name;
	directory.Files = AllocatePool(fileAmount * sizeof(File));
	directory.FileAmount = fileAmount;
	directory.Directories = AllocatePool(directoryAmount * sizeof(Directory));
	directory.DirectoryAmount = directoryAmount;

	return directory;
}

UINT64 file_size(EFI_FILE* FileHandle)
{
	UINT64 ret;
	EFI_FILE_INFO *FileInfo;

	FileInfo = get_file_info(FileHandle);
	ret = FileInfo->FileSize;
	FreePool(FileInfo);
	return ret;
}

File read_file(EFI_FILE* directory, CHAR16* path)
{
	EFI_FILE_HANDLE fileHandle;
	
	/* open the file */
	uefi_call_wrapper(directory->Open, 5, directory, &fileHandle, path, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM);

	UINT64 size = file_size(fileHandle);
	UINT8* data = AllocatePool(size);
	
	uefi_call_wrapper(fileHandle->Read, 3, fileHandle, &size, data);

	File output;
	output.Name = char16_to_char(path);
	output.Data = data;
	output.Size = size;

  	uefi_call_wrapper(fileHandle->Close, 1, fileHandle);

	return output;
}

Directory read_directory(EFI_FILE* efiDirectory)
{
	EFI_FILE_INFO* directoryInfo = get_file_info(efiDirectory);

	Directory out;
	out.Name = char16_to_char(directoryInfo->FileName);
	out.Files = 0;
	out.FileAmount = 0;
	out.Directories = 0;
	out.DirectoryAmount = 0;

	FreePool(directoryInfo);

	while (1) 
	{
		EFI_FILE_INFO *FileInfo;
		UINTN FileInfoSize = 0;

		EFI_STATUS status = efiDirectory->Read(efiDirectory, &FileInfoSize, NULL);
        if (status != EFI_BUFFER_TOO_SMALL) 
		{
            break;
		}

		status = SystemTable->BootServices->AllocatePool(EfiBootServicesData, FileInfoSize, (void**)&FileInfo);
		if (EFI_ERROR(status)) 
		{
			Print(L"Error allocating memory for file info\n");
			break;
		}

		status = efiDirectory->Read(efiDirectory, &FileInfoSize, FileInfo);
		if (EFI_ERROR(status)) 
		{
			Print(L"Error reading file info\n");
			SystemTable->BootServices->FreePool(FileInfo);
			break;
		}


		if (FileInfo->Attribute & EFI_FILE_DIRECTORY) 
		{
			if (StrCmp(FileInfo->FileName, L".") != 0 && StrCmp(FileInfo->FileName, L"..") != 0) 
			{
				EFI_FILE_PROTOCOL *efiSubDirectory;
				status = efiDirectory->Open(efiDirectory, &efiSubDirectory, FileInfo->FileName, EFI_FILE_MODE_READ, EFI_FILE_DIRECTORY);
				if (!EFI_ERROR(status)) 
				{	
					Directory newDirectory = read_directory(efiSubDirectory);
					
					Directory* newDirectoryArray = AllocatePool(sizeof(Directory) * (out.DirectoryAmount + 1));
					if (out.DirectoryAmount != 0)
					{
						CopyMem(newDirectoryArray, out.Directories, sizeof(Directory) * out.DirectoryAmount);
						FreePool(out.Directories);
					}
					out.Directories = newDirectoryArray;
					out.Directories[out.DirectoryAmount] = newDirectory;
					out.DirectoryAmount++;

					efiSubDirectory->Close(efiSubDirectory);
				}
			}
		}
		else
		{
			File newFile = read_file(efiDirectory, FileInfo->FileName);
			
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
		SystemTable->BootServices->FreePool(FileInfo);
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

	EFI_FILE* kernelDirectory = load_efi_file(NULL, L"KERNEL");
	EFI_FILE* fontsDirectory = load_efi_file(NULL, L"FONTS");

	Elf64_Ehdr kernelFile = load_elf_file(kernelDirectory, L"Kernel.elf");

	PSF_FONT ttyFont = load_psf_font(fontsDirectory, L"zap-vga16.psf");

	kernelDirectory->Close(kernelDirectory);
	fontsDirectory->Close(fontsDirectory);

	Directory rootDirectory = read_directory(load_efi_file(NULL, L"."));
	
	Framebuffer screenbuffer = get_gop_framebuffer();
	EFI_MEMORY_MAP memoryMap = get_memory_map();
	void* rsdp = get_rsdp();

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

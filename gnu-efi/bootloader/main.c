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

int memcmp(const void* aptr, const void* bptr, size_t n){
	const unsigned char* a = aptr, *b = bptr;
	for (size_t i = 0; i < n; i++){
		if (a[i] < b[i]) return -1;
		else if (a[i] > b[i]) return 1;
	}
	return 0;
}

EFI_FILE* LoadFile(EFI_FILE* Directory, CHAR16* Path)
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

PSF_FONT LoadPSFFont(EFI_FILE* Directory, CHAR16* Path)
{
	EFI_FILE* Font = LoadFile(Directory, Path);

	if (Font == NULL)
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
	Font->Read(Font, &size, FontHeader);

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
	Font->SetPosition(Font, sizeof(PSF_HEADER));
	SystemTable->BootServices->AllocatePool(EfiLoaderData, GlyphBufferSize, (void**)&GlyphBuffer);
	Font->Read(Font, &GlyphBufferSize, GlyphBuffer);

	PSF_FONT newFont;

	newFont.PSF_Header = FontHeader;
	newFont.glyphBuffer = (char*)GlyphBuffer;

	Print(L"FONT INFO\n\r");
	Print(L"Char Size: %d\n\r", newFont.PSF_Header->charsize);
	Print(L"Mode: 0x%x\n\r", newFont.PSF_Header->mode);
	Print(L"FONT INFO END\n\r");

	return newFont;
}

Elf64_Ehdr LoadELFFile(EFI_FILE* Directory, CHAR16* Path)
{
	EFI_FILE* File = LoadFile(Directory, Path);
	if (File == NULL)
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
	File->GetInfo(File, &gEfiFileInfoGuid, &FileInfoSize, NULL);
	SystemTable->BootServices->AllocatePool(EfiLoaderData, FileInfoSize, (void**)&FileInfo);
	File->GetInfo(File, &gEfiFileInfoGuid, &FileInfoSize, (void**)&FileInfo);
	UINTN Size = sizeof(Header);
	File->Read(File, &Size, &Header);

	if (memcmp(&Header.e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0 ||
		Header.e_ident[EI_CLASS] != ELFCLASS64 ||
		Header.e_ident[EI_DATA] != ELFDATA2LSB ||
		Header.e_machine != EM_X86_64 ||
		Header.e_version != EV_CURRENT)
	{
		Print(L"ERROR: %s is corrupted!", Path);
	}

	Elf64_Phdr* phdrs;
	File->SetPosition(File, Header.e_phoff);
	UINTN size = Header.e_phnum * Header.e_phentsize;
	SystemTable->BootServices->AllocatePool(EfiLoaderData, size, (void**)&phdrs);
	File->Read(File, &size, phdrs);

	for (Elf64_Phdr* phdr = phdrs; (char*)phdr < (char*)phdrs + Header.e_phnum * Header.e_phentsize; phdr = (Elf64_Phdr*)((char*)phdr + Header.e_phentsize))
	{
		switch (phdr->p_type)
		{
		case PT_LOAD:
		{
			int pages = (phdr->p_memsz * 0x1000 - 1) / 0x1000;
			Elf64_Addr segment = phdr->p_paddr;
			SystemTable->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, pages, &segment);

			File->SetPosition(File, phdr->p_offset);
			UINTN size = phdr->p_filesz;
			File->Read(File, &size, (void*)segment);
		}
		break;
		}
	}

	return Header;
}

Framebuffer GetFramebuffer()
{
	EFI_GUID GOP_GUID = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	EFI_GRAPHICS_OUTPUT_PROTOCOL* GOP;
	EFI_STATUS Status = uefi_call_wrapper(BS->LocateProtocol, 3, &GOP_GUID, NULL, (void**)&GOP);

	Print(L"Initializing GOP..\n\r");
	if (EFI_ERROR(Status))
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

EFI_MEMORY_MAP GetMemoryMap()
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

void* GetRSDP()
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

Directory CreateDirectory(const char* name, uint64_t fileAmount, uint64_t directoryAmount)
{
	Directory directory;
	directory.Name = name;
	directory.Files = AllocatePool(fileAmount * sizeof(File));
	directory.FileAmount = fileAmount;
	directory.Directories = AllocatePool(directoryAmount * sizeof(Directory));
	directory.DirectoryAmount = directoryAmount;

	return directory;
}

UINT64 FileSize(EFI_FILE_HANDLE FileHandle)
{
	UINT64 ret;
	EFI_FILE_INFO *FileInfo;

	FileInfo = LibFileInfo(FileHandle);
	ret = FileInfo->FileSize;
	FreePool(FileInfo);
	return ret;
}

File ReadFile(EFI_FILE* directory, CHAR16* path, char* fileName)
{
	EFI_FILE_HANDLE fileHandle;
	
	/* open the file */
	uefi_call_wrapper(directory->Open, 5, directory, &fileHandle, path, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM);

	UINT64 size = FileSize(fileHandle);
	UINT8* data = AllocatePool(size);
	
	uefi_call_wrapper(fileHandle->Read, 3, fileHandle, &size, data);

	File output;
	output.Name = fileName;
	output.Data = data;
	output.Size = size;

  	uefi_call_wrapper(fileHandle->Close, 1, fileHandle);

	return output;
}

typedef struct
{
	Framebuffer* Screenbuffer;
	PSF_FONT** PSFFonts;	
	uint8_t FontAmount;
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

	EFI_FILE* KernelDir = LoadFile(NULL, L"KERNEL");
	EFI_FILE* RootDir = LoadFile(NULL, L"ROOT");
	EFI_FILE* FontsDir = LoadFile(RootDir, L"FONTS");

	Elf64_Ehdr KernelELF = LoadELFFile(KernelDir, L"Kernel.elf");

	PSF_FONT FontVGA = LoadPSFFont(FontsDir, L"zap-vga16.psf");
	PSF_FONT FontLight = LoadPSFFont(FontsDir, L"zap-light16.psf");
	PSF_FONT* Fonts[] = {&FontVGA, &FontLight};

	Directory rootDirectory = CreateDirectory("root", 0, 1);
	Directory fontDirectory = CreateDirectory("fonts", 2, 0);
	rootDirectory.Directories[0] = fontDirectory;

	fontDirectory.Files[0] = ReadFile(FontsDir, L"zap-vga16.psf", "zap-vga16.psf");
	fontDirectory.Files[1] = ReadFile(FontsDir, L"zap-light16.psf", "zap-light16.psf");

	Framebuffer screenbuffer = GetFramebuffer();
	EFI_MEMORY_MAP memoryMap = GetMemoryMap();
	void* RSDP = GetRSDP();

	Print(L"Exiting boot services...\n\r");
	SystemTable->BootServices->ExitBootServices(ImageHandle, memoryMap.Key);

	BootInfo bootInfo;
	bootInfo.Screenbuffer = &screenbuffer;
	bootInfo.PSFFonts = Fonts;
	bootInfo.FontAmount = sizeof(Fonts)/sizeof(Fonts[0]);	
	bootInfo.MemoryMap = &memoryMap;
	bootInfo.RSDP = RSDP;
	bootInfo.RT = SystemTable->RuntimeServices;
	bootInfo.RootDirectory = &rootDirectory;

	void (*KernelMain)(BootInfo*) = ((__attribute__((sysv_abi)) void (*)(BootInfo*)) KernelELF.e_entry);

	Print(L"Entering Kernel...\n\r");
	KernelMain(&bootInfo);

	return EFI_SUCCESS; // Exit the UEFI application
}

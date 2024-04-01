#pragma once

#include <stdint.h>

#ifdef __KERNEL__
typedef struct 
{
	uint32_t type;
	uint32_t pad;
	void* physicalStart;
	void* virtualStart;
	uint64_t amountOfPages;
	uint64_t attribute;
} EfiMemoryDescriptor;

#define EFI_RESERVED 0
#define EFI_LOADER_CODE 1 
#define EFI_LOADER_DATA 2
#define EFI_BOOT_SERVICES_CODE 3
#define EFI_BOOT_SERVICES_DATA 4
#define EFI_RUNTIME_SERVICES_CODE 5
#define EFI_RUNTIME_SERVICES_DATA 6
#define EFI_CONVENTIONAL_MEMORY 7
#define EFI_UNUSABLE_MEMORY 8
#define EFI_ACPI_RECLAIM_MEMORY 9
#define EFI_ACPI_MEMORY_NVS 10
#define EFI_MEMORY_MAPPED_IO 11
#define EFI_MEMORY_MAPPED_IO_PORT_SPACE 12
#define EFI_PAL_CODE 13
#define EFI_PERSISTENT_MEMORY 14 
#else
#include <efi.h>
#include <efilib.h>
typedef EFI_MEMORY_DESCRIPTOR EfiMemoryDescriptor;
#endif

#define EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, index) (EfiMemoryDescriptor*)((uint64_t)(memoryMap)->base + ((index) * (memoryMap)->descriptorSize))

#define EFI_MEMORY_TYPE_KERNEL 0x80000000
#define EFI_MEMORY_TYPE_PAGE_TABLE 0x80000001
#define EFI_MEMORY_TYPE_BOOT_INFO 0x80000002
#define EFI_MEMORY_TYPE_RAM_DISK 0x80000003
#define EFI_MEMORY_TYPE_MEMORY_MAP 0x80000004

typedef struct
{
	EfiMemoryDescriptor* base;
	uint64_t descriptorAmount;
	uint64_t key;
	uint64_t descriptorSize;
	uint32_t descriptorVersion;
} EfiMemoryMap;

typedef struct
{
    uint32_t* base;
	uint64_t size;
	uint32_t width;
	uint32_t height;
	uint32_t pixelsPerScanline;
} GopBuffer;

typedef struct __attribute__((packed))
{ 
	uint16_t magic;
	uint8_t mode;
	uint8_t charSize;
} PsfHeader;

typedef struct
{
	PsfHeader header;
	uint64_t glyphsSize;
	void* glyphs;
} PsfFont;

typedef struct RamFile
{
	char name[32];
	void* data;
	uint64_t size;
	struct RamFile* next;
	struct RamFile* prev;
} RamFile;

typedef struct RamDir
{
	char name[32];
	RamFile* firstFile;
	RamFile* lastFile;
	struct RamDir* firstChild;
	struct RamDir* lastChild;
	struct RamDir* next;
	struct RamDir* prev;
} RamDir;

typedef struct 
{    
	EfiMemoryMap memoryMap;
	GopBuffer gopBuffer;
	PsfFont font;
	RamDir* ramRoot;
	void* rsdp;
	void* runtimeServices;
} BootInfo;

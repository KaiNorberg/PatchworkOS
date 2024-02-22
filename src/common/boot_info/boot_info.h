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
#else
#include <efilib.h>
typedef EFI_MEMORY_DESCRIPTOR EfiMemoryDescriptor;
#endif

typedef struct
{
	EfiMemoryDescriptor* base;
	uint64_t descriptorAmount;
	uint64_t key;
	uint64_t descriptorSize;
	uint32_t descriptorVersion;
} EfiMemoryMap;

typedef struct __attribute__((packed))
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

typedef struct RamDirectory
{
	char name[32];
	RamFile* firstFile;
	RamFile* lastFile;
	struct RamDirectory* firstChild;
	struct RamDirectory* lastChild;
	struct RamDirectory* next;
	struct RamDirectory* prev;
} RamDirectory;

typedef struct 
{    
    void* physicalAddress;
	EfiMemoryMap memoryMap;
	GopBuffer gopBuffer;
	PsfFont font;
	RamDirectory* ramRoot;
	void* rsdp;
	void* runtimeServices;
} BootInfo;

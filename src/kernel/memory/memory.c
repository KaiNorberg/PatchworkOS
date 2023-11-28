#include "memory.h"

const char* EFI_MEMORY_TYPE_STRINGS[] =
{
	"EFI_RESERVED_MEMORY_TYPE",
	"EFI_LOADER_CODE",
	"EFI_LOADER_DATA",
	"EFI_BOOT_SERVICES_CODE",
	"EFI_BOOT_SERVICES_DATA",
	"EFI_RUNTIME_SERVICES_CODE",
	"EFI_RUNTIME_SERVICES_DATA",
	"EFI_CONVENTIONAL_MEMORY",
	"EFI_UNUSABLE_MEMORY",
	"EFI_ACPI_RECLAIM_MEMORY",
	"EFI_ACPI_MEMORY_NVS",
	"EFI_MEMORY_MAPPED_IO",
	"EFI_MEMORY_MAPPED_IO_PORT_SPACE",
	"EFI_PAL_CODE",
	"EFI_PERSISTENT_MEMORY",
	"EFI_MAX_MEMORY_TYPE"
};

uint8_t is_memory_type_reserved(uint64_t memoryType)
{
	switch (memoryType)
	{
	case EFI_CONVENTIONAL_MEMORY:
	case EFI_ACPI_RECLAIM_MEMORY:
	case EFI_BOOT_SERVICES_CODE:
	case EFI_BOOT_SERVICES_DATA:
	case EFI_LOADER_CODE:
	case EFI_LOADER_DATA:
	{
		return 0;
	}
	break;
	default:
	{
		return 1;
	}
	break;
	}
}

/*int8_t is_memory_type_reserved(uint64_t memoryType)
{
	switch (memoryType)
	{
	case EFI_CONVENTIONAL_MEMORY:
	case EFI_ACPI_RECLAIM_MEMORY:
	case EFI_BOOT_SERVICES_CODE:
	case EFI_BOOT_SERVICES_DATA:
	case EFI_LOADER_CODE:
	case EFI_LOADER_DATA:
	case EFI_RUNTIME_SERVICES_CODE:
	case EFI_RUNTIME_SERVICES_DATA:
	case EFI_MAX_MEMORY_TYPE:
	{
		return 0;
	}
	break;
	default:
	{
		return 1;
	}
	break;
	}
}*/
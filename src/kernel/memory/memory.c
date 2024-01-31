#include "memory.h"

uint8_t is_memory_type_reserved(uint64_t memoryType)
{
	switch (memoryType)
	{
	case EFI_CONVENTIONAL_MEMORY:
	case EFI_LOADER_CODE:
	case EFI_LOADER_DATA:
	case EFI_BOOT_SERVICES_CODE:
	case EFI_BOOT_SERVICES_DATA:
	case EFI_PERSISTENT_MEMORY:
	{
		return 0;
	}
	default:
	{
		return 1;
	}
	}
}
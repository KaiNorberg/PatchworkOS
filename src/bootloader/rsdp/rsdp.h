#pragma once

#include <efi.h>
#include <efilib.h>

void* rsdp_get(EFI_SYSTEM_TABLE* systemTable);
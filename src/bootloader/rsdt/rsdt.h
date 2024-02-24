#pragma once

#include <efi.h>
#include <efilib.h>

#include "efiapi.h"

void* rsdt_get(EFI_SYSTEM_TABLE* systemTable);
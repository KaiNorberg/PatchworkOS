#pragma once

#include <efi.h>
#include <efilib.h>

#include "gop/gop.h"
#include "psf/psf.h"
#include "memory/memory.h"
#include "ram_disk/ram_disk.h"

#include <common/common.h>
#include <common/boot_info/boot_info.h>

void loader_load_kernel(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* systemTable, BootInfo* bootInfo);

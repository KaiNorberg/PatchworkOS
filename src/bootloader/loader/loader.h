#pragma once

#include <efi.h>
#include <efilib.h>

#include <common/common.h>
#include <common/boot_info/boot_info.h>

#include "gop/gop.h"
#include "psf/psf.h"
#include "memory/memory.h"
#include "ram_disk/ram_disk.h"

void* load_kernel(CHAR16* path, EFI_HANDLE imageHandle);

void jump_to_kernel(void* entry, BootInfo* bootInfo);
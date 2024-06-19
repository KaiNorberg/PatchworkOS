#pragma once

#include <efi.h>
#include <efilib.h>

#include <common/boot_info.h>

#include "gop.h"
#include "mem.h"
#include "psf.h"
#include "ram_disk.h"

void* load_kernel(CHAR16* path, EFI_HANDLE imageHandle);

void jump_to_kernel(void* entry, boot_info_t* bootInfo);

#pragma once

#include <boot/boot_info.h>
#include <efi.h>
#include <efilib.h>

#define GOP_WIDTH 1920
#define GOP_HEIGHT 1080
#define GOP_USE_DEFAULT_RES 1

EFI_STATUS gop_buffer_init(boot_gop_t* buffer);

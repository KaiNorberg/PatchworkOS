#pragma once

#include <bootloader/boot_info.h>
#include <efi.h>
#include <efilib.h>

#define GOP_WIDTH 1920
#define GOP_HEIGHT 1080

void gop_buffer_init(gop_buffer_t* buffer);

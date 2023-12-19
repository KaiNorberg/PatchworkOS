#pragma once

#include <efi.h>
#include <efilib.h>

typedef struct
{
	uint32_t* base;
	uint64_t size;
	uint32_t width;
	uint32_t height;
	uint32_t pixelsPerScanline;
} Framebuffer;

void gop_get_framebuffer(Framebuffer* framebuffer);
#pragma once

#include <stdint.h>

typedef struct
{
	uint16_t Magic;
	uint8_t Mode;
	uint8_t CharSize;
} PSFHeader;

typedef struct
{
	PSFHeader* Header;
	void* Glyphs;
} PSFFont;

#pragma once

#include <stdint.h>

typedef struct
{
	uint16_t magic;
	uint8_t mode;
	uint8_t charSize;
} PSFHeader;

typedef struct
{
	PSFHeader* header;
	void* glyphs;
} PSFFont;

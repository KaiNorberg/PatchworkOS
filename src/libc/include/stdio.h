#pragma once

#include <stdint.h>
#include <stddef.h>

typedef void FILE;

FILE* fopen(const char* filename, const char* mode);
int fgetc(FILE* stream);
size_t fread(void* buffer, size_t size, size_t count, FILE* stream);
int fclose(FILE* stream);
	
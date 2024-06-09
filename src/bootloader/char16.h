#pragma once

#include <stddef.h>
#include <stdint.h>

#include <efi.h>
#include <efilib.h>

void char16_to_char(CHAR16* string, char* out);
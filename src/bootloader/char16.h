#pragma once

#include <stdint.h>
#include <stddef.h>

#include <efi.h>
#include <efilib.h>

void char16_to_char(CHAR16* string, char* out);
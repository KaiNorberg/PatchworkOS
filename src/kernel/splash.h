#pragma once

#include "defs.h"

#include <sys/gfx.h>

#include <common/boot_info.h>

#define SPLASH_WIDTH 400
#define SPLASH_HEIGHT 500
#define SPLASH_SHADOW_OFFSET 1
#define SPLASH_NAME_SCALE 3
#define SPLASH_NAME_OFFSET 150
#define SPLASH_VERSION_SCALE 2
#define SPLASH_VERSION_OFFSET (SPLASH_NAME_OFFSET - 50)

#define SPLASH_MESSAGE_SCALE 2
#define SPLASH_MESSAGE_OFFSET (-100)

#define SPLASH_FUNC() splash_print(__FUNCTION__, 0xFF000000)
#define SPLASH_ASSERT(condition, msg) if (!(condition)) { splash_print("err: " msg, 0xFFFF0000); while(1) { asm volatile("hlt"); } }

void splash_init(GopBuffer* gopBuffer, BootFont* screenFont);

void splash_print(const char* string, pixel_t color);

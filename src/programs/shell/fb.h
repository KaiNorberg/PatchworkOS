#pragma once

#include <stdint.h>

#define FB_ADDR ((void*)0xF0000000)

#define FB_FONT "B:/fonts/zap-vga16.psf"

#define FB_CHAR_HEIGHT 16
#define FB_CHAR_WIDTH 8

#define PSF_MAGIC 1078

typedef struct __attribute__((packed))
{ 
    uint16_t magic;
    uint8_t mode;
    uint8_t charSize;
} PsfHeader;

void fb_init();

void fb_scroll(uint64_t offset);

void fb_char(char chr, uint64_t x, uint64_t y, uint32_t foreground, uint32_t background);

uint64_t fb_width();

uint64_t fb_height();
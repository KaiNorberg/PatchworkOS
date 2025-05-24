#ifndef GRF_H
#define GRF_H

#include <stdint.h>

// This file contains definitions for the grf font format, a font format originally made for PatchworkOS.
// Source: https://github.com/KaiNorberg/grf

#if defined(_MSC_VER)
#define GRF_PACK_START __pragma(pack(push, 1))
#define GRF_PACK_END __pragma(pack(pop))
#define GRF_PACK_STRUCT
#elif defined(__GNUC__) || defined(__clang__)
#define GRF_PACK_START
#define GRF_PACK_END
#define GRF_PACK_STRUCT __attribute__((packed))
#else
#warning "Unsupported compiler: Structures might not be packed correctly."
#define GRF_PACK_START
#define GRF_PACK_END
#define GRF_PACK_STRUCT
#endif

#define GRF_MAGIC 0x47524630 // ASCII for "GRF0"
#define GRF_NONE UINT32_MAX

GRF_PACK_START
typedef struct GRF_PACK_STRUCT
{
    uint32_t magic;             // GRF0.
    int16_t ascender;           // Font ascender in pixels.
    int16_t descender;          // Font descender in pixels.
    int16_t height;             // Total line height in pixels.
    uint32_t glyphOffsets[256]; // Offsets to each grf_glyph_t in grf_t::buffer, indexed by ascii chars. GRF_NONE means
                                // "none".
    uint32_t kernOffsets[256]; // Offsets to each grf_kern_block_t in grf_t::buffer, indexed by the starting ascii char.
                               // GRF_NONE means "none".
    uint8_t buffer[]; // Glyphs and kernel info is stored here. No guarantee of glyph or kerning orders, could be one
                      // after the other, interleaved, etc, allways use the offsets.
} grf_t;

typedef struct GRF_PACK_STRUCT
{
    int16_t bearingX; // Horizontal bearing.
    int16_t bearingY; // Vertical bearing.
    int16_t advanceX; // Horizontal advance.
    int16_t advanceY; // Vertical advance, usually 0.
    uint16_t width;   // The width of the buffer in pixels/bytes.
    uint16_t height;  // The height of the buffer in pixels/bytes.
    uint8_t buffer[]; // The pixel buffer, each pixel is 1 byte.
} grf_glyph_t;

typedef struct GRF_PACK_STRUCT
{
    uint8_t secondChar; // The second character in the kerning pair
    int16_t offsetX;    // The horizontal offset to be added to advanceX for this character pair.
    int16_t offsetY;    // The vertical to be added to advanceY for this character pair, probably 0.
} grf_kern_entry_t;

typedef struct GRF_PACK_STRUCT
{
    uint16_t amount;            // The amount of kerning entries for this char
    grf_kern_entry_t entries[]; // The entries, these entries will always be sorted by char, so the entry for 'A' is
                                // before 'B', this allows for O(log N) look ups.
} grf_kern_block_t;
GRF_PACK_END

#endif // GRF_H
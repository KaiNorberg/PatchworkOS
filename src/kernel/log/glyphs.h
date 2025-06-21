#include <stdint.h>

// Glyph data generated using tools/generate_glyph_cache.

#define GLYPH_HEIGHT 16
#define GLYPH_WIDTH 8
#define GLYPH_AMOUNT 256

typedef struct
{
    uint32_t pixels[GLYPH_HEIGHT * GLYPH_WIDTH];
} glyph_t;

typedef struct
{
    glyph_t glyphs[GLYPH_AMOUNT];
} glyph_cache_t;

const glyph_cache_t* glyph_cache_get(void);
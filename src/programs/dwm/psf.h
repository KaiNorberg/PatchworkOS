#pragma once

#include <libdwm/font_id.h>
#include <stdint.h>
#include <sys/list.h>

#define PSF1_MAGIC 0x0436
#define PSF2_MAGIC 0x864AB572
#define PSF1_MODE_512 (1 << 0)

#define FONT_DIR "home:/theme/fonts"

typedef struct
{
    list_entry_t entry;
    font_id_t id;
    uint32_t width;
    uint32_t height;
    uint32_t scale;
    uint32_t glyphSize;
    uint32_t glyphAmount;
    uint8_t glyphs[];
} psf_t;

psf_t* psf_new(const char* path, uint32_t desiredHeight);

void psf_free(psf_t* psf);

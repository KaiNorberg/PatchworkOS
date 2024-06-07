#ifndef _SYS_WIN_H
#define _SYS_WIN_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#include "../_AUX/config.h"
#include "../_AUX/ERR.h"
#include "../_AUX/pixel_t.h"

typedef struct win_info 
{
    uint64_t width;
    uint64_t height;
    uint64_t x;
    uint64_t y;
} win_info_t;

#define WIN_SIZE(info) ((info)->width * (info)->height * sizeof(pixel_t))

#if defined(__cplusplus)
}
#endif

#endif
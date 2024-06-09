#ifndef _SYS_WIN_H
#define _SYS_WIN_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "../_AUX/ERR.h"
#include "../_AUX/config.h"
#include "../_AUX/fd_t.h"
#include "../_AUX/pixel_t.h"
#include "../_AUX/point_t.h"
#include "../_AUX/rect_t.h"

typedef struct win_info
{
    uint64_t width;
    uint64_t height;
    uint64_t x;
    uint64_t y;
} win_info_t;

#define WIN_SIZE(info) ((info)->width * (info)->height * sizeof(pixel_t))

uint64_t flush(fd_t fd, const void* buffer, uint64_t size, const rect_t* rect);

#if defined(__cplusplus)
}
#endif

#endif
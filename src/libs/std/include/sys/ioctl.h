#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#include "../_AUX/config.h"
#include "../_AUX/fd_t.h"
#include "../_AUX/ERR.h"

typedef struct ioctl_fb_info
{
    uint64_t size;
    uint64_t width;
    uint64_t height;
    uint64_t pixelsPerScanline;
    uint8_t bytesPerPixel;
    uint8_t blueOffset;
    uint8_t greenOffset;
    uint8_t redOffset;
    uint8_t alphaOffset;
} ioctl_fb_info_t;

#define IOCTL_FB_INFO 0

uint64_t ioctl(fd_t fd, uint64_t request, void* buffer, uint64_t length);

#if defined(__cplusplus)
}
#endif

#endif
#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#include "../_AUX/config.h"
#include "../_AUX/fd_t.h"

//Use different struct naming convention for stdlib?
typedef struct ioctl_fb_info
{
    uint64_t width;
    uint64_t height;
} ioctl_fb_info_t;

#define IOCTL_FB_GET_INFO 0

_PUBLIC uint64_t ioctl(fd_t fd, uint64_t request, void* buffer);

#if defined(__cplusplus)
}
#endif

#endif
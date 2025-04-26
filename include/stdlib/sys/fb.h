#ifndef _SYS_FB_H
#define _SYS_FB_H 1

// Should be followed by the buffer to be flushed.
typedef struct ioctl_fb_flush
{
    uint64_t x;
    uint64_t y;
    uint64_t height;
    uint64_t width;
    uint64_t stride;
} ioctl_fb_flush_t;

#define IOCTL_FB_FLUSH 1234

#endif

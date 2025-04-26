#include "gop.h"

#include "fb.h"
#include "defs.h"
#include "log.h"
#include "sched.h"

#include <errno.h>

static uint64_t gop_flush(fb_t* fb, const fb_pixel_t* buffer, uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint64_t stride)
{
    return ERROR(EIMPL);
}

static fb_t fb =
{
    .width = 0, // Updated in gop_init
    .height = 0, // Updated in gop_init
    .flush = gop_flush,
};

void gop_init(gop_buffer_t* gopBuffer)
{
    fb.width = gopBuffer->width;
    fb.height = gopBuffer->height;
    //ASSERT_PANIC(fb_expose(&fb) != ERR);
}

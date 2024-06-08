#ifndef _AUX_RECT_T_H
#define _AUX_RECT_T_H 1

#include <stdint.h>

typedef struct rect
{
    uint64_t x;
    uint64_t y;
    uint64_t width;
    uint64_t height;
} rect_t;

#endif
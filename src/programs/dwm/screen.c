#include "screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fb.h>
#include <sys/io.h>
#include <sys/proc.h>

static fb_info_t info;
static void* frontbuffer;

static gfx_t backbuffer;

static tiles_t tiles;

static void frontbuffer_init(void)
{
    fd_t fb = open("sys:/fb0");
    if (fb == ERR)
    {
        exit(EXIT_FAILURE);
    }
    if (ioctl(fb, IOCTL_FB_INFO, &info, sizeof(fb_info_t)) == ERR)
    {
        exit(EXIT_FAILURE);
    }

    switch (info.format)
    {
    case FB_ARGB32:
    {
        frontbuffer = mmap(fb, NULL, info.stride * info.height * sizeof(uint32_t), PROT_READ | PROT_WRITE);
        if (frontbuffer == NULL)
        {
            exit(EXIT_FAILURE);
        }
        memset(frontbuffer, 0, info.stride * info.height * sizeof(uint32_t));
    }
    break;
    default:
    {
        exit(EXIT_FAILURE);
    }
    }

    close(fb);
}

static void backbuffer_init(void)
{
    backbuffer.buffer = malloc(info.stride * info.height * sizeof(pixel_t));
    if (backbuffer.buffer == NULL)
    {
        exit(EXIT_FAILURE);
    }
    backbuffer.width = info.width;
    backbuffer.height = info.height;
    backbuffer.stride = info.stride;
    backbuffer.invalidRect = (rect_t){0};
    memset(backbuffer.buffer, 0, info.width * info.height * sizeof(pixel_t));
}

static void tiles_init(void)
{
    tiles.columns = (info.width + TILE_SIZE - 1) / TILE_SIZE;
    tiles.rows = (info.height + TILE_SIZE - 1) / TILE_SIZE;
    tiles.totalAmount = tiles.columns * tiles.rows;
    tiles.invalidAmount = 0;

    tiles.map = calloc(tiles.totalAmount, sizeof(bool));
    if (tiles.map == NULL)
    {
        exit(EXIT_FAILURE);
    }
    tiles.indices = calloc(tiles.totalAmount, sizeof(uint64_t));
    if (tiles.indices == NULL)
    {
        exit(EXIT_FAILURE);
    }
}

static void tiles_invalidate_rect(const rect_t* rect)
{
    uint64_t startColumn = rect->left / TILE_SIZE;
    uint64_t startRow = rect->top / TILE_SIZE;
    uint64_t endColumn = (rect->right - 1) / TILE_SIZE;
    uint64_t endRow = (rect->bottom - 1) / TILE_SIZE;

    startColumn = MIN(startColumn, tiles.columns - 1);
    startRow = MIN(startRow, tiles.rows - 1);
    endColumn = MIN(endColumn, tiles.columns - 1);
    endRow = MIN(endRow, tiles.rows - 1);

    for (uint64_t row = startRow; row <= endRow; row++)
    {
        for (uint64_t column = startColumn; column <= endColumn; column++)
        {
            uint64_t i = row * tiles.columns + column;
            if (!tiles.map[i])
            {
                tiles.map[i] = true;
                tiles.indices[tiles.invalidAmount] = i;
                tiles.invalidAmount++;
            }
        }
    }
}

static void tiles_index_to_rect(uint64_t index, rect_t* rect)
{
    uint64_t column = index % tiles.columns;
    uint64_t row = index / tiles.columns;

    rect->left = column * TILE_SIZE;
    rect->top = row * TILE_SIZE;
    rect->right = MIN((uint64_t)rect->left + TILE_SIZE, info.width);
    rect->bottom = MIN((uint64_t)rect->top + TILE_SIZE, info.height);
}

static void tiles_clear(void)
{
    for (uint64_t i = 0; i < tiles.invalidAmount; i++)
    {
        tiles.map[tiles.indices[i]] = false;
    }
    tiles.invalidAmount = 0;
}

void screen_init(void)
{
    frontbuffer_init();
    backbuffer_init();
    tiles_init();
}

void screen_deinit(void)
{
    free(tiles.map);
    free(tiles.indices);
    free(backbuffer.buffer);
    munmap(frontbuffer, info.stride * info.height * sizeof(uint32_t));
}

void screen_transfer(surface_t* surface, const rect_t* rect)
{
    point_t srcPoint = {
        .x = MAX(rect->left - surface->pos.x, 0),
        .y = MAX(rect->top - surface->pos.y, 0),
    };
    gfx_transfer(&backbuffer, &surface->gfx, rect, &srcPoint);
    tiles_invalidate_rect(rect);
}

void screen_transfer_blend(surface_t* surface, const rect_t* rect)
{
    point_t srcPoint = {
        .x = MAX(rect->left - surface->pos.x, 0),
        .y = MAX(rect->top - surface->pos.y, 0),
    };
    gfx_transfer_blend(&backbuffer, &surface->gfx, rect, &srcPoint);
    tiles_invalidate_rect(rect);
}

void screen_swap(void)
{
    switch (info.format)
    {
    case FB_ARGB32:
    {
        for (uint64_t i = 0; i < tiles.invalidAmount; i++)
        {
            rect_t rect;
            tiles_index_to_rect(tiles.indices[i], &rect);

            for (int64_t y = 0; y < RECT_HEIGHT(&rect); y++)
            {
                uint64_t offset = (rect.left * sizeof(pixel_t)) + (y + rect.top) * sizeof(pixel_t) * info.stride;

                memcpy((void*)((uint64_t)frontbuffer + offset), (void*)((uint64_t)backbuffer.buffer + offset),
                    RECT_WIDTH(&rect) * sizeof(pixel_t));
            }
        }
    }
    break;
    default:
    {
        exit(EXIT_FAILURE);
    }
    }
    tiles_clear();
}

uint64_t screen_width(void)
{
    return info.width;
}

uint64_t screen_height(void)
{
    return info.height;
}

void screen_rect(rect_t* rect)
{
    *rect = RECT_INIT_DIM(0, 0, info.width, info.height);
}

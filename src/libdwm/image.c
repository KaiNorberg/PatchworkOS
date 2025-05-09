#include "internal.h"

#include <stdlib.h>

#define FBMP_MAGIC 0x706D6266

typedef struct fbmp
{
    uint32_t magic;
    uint32_t width;
    uint32_t height;
    pixel_t data[];
} fbmp_t;

image_t* image_new_blank(display_t* disp, uint64_t width, uint64_t height)
{
    image_t* image = malloc(sizeof(image_t));
    if (image == NULL)
    {
        return NULL;
    }

    image->disp = disp;
    image->surface = display_gen_id(disp);
    image->width = width;
    image->height = height;

    image->draw.disp = image->disp;
    image->draw.surface = image->surface;
    image->draw.drawArea = RECT_INIT_DIM(0, 0, width, height);
    image->draw.invalidRect = (rect_t){0};

    cmd_surface_new_t* cmd = display_cmds_push(disp, CMD_SURFACE_NEW, sizeof(cmd_surface_new_t));
    cmd->id = image->surface;
    cmd->type = SURFACE_HIDDEN;
    cmd->rect = image->draw.drawArea;
    display_cmds_flush(disp);

    return image;
}

fbmp_t* fbmp_new(const char* path)
{
    fd_t file = open(path);
    if (file == ERR)
    {
        return NULL;
    }

    uint64_t fileSize = seek(file, 0, SEEK_END);
    seek(file, 0, SEEK_SET);

    fbmp_t* fbmp = malloc(fileSize);
    if (fbmp == NULL)
    {
        close(file);
        return NULL;
    }

    if (read(file, fbmp, fileSize) != fileSize)
    {
        close(file);
        free(fbmp);
        return NULL;
    }

    close(file);

    if (fbmp->magic != FBMP_MAGIC)
    {
        free(fbmp);
        return NULL;
    }

    return fbmp;
}

image_t* image_new(display_t* disp, const char* path)
{
    fbmp_t* fbmp = fbmp_new(path);
    if (fbmp == NULL)
    {
        return NULL;
    }

    image_t* image = image_new_blank(disp, fbmp->width, fbmp->height);
    if (image == NULL)
    {
        free(fbmp);
        return NULL;
    }

    uint64_t maxLength = (CMD_BUFFER_MAX_DATA - sizeof(cmd_draw_buffer_t)) / sizeof(pixel_t);
    uint64_t index = 0;
    while (1)
    {
        uint64_t length = MIN(maxLength, fbmp->width * fbmp->height - index);
        if (length == 0)
        {
            break;
        }

        draw_buffer(&image->draw, &fbmp->data[index], index, length);

        index += length;
    }
    display_cmds_flush(disp);

    free(fbmp);
    return image;
}

void image_free(image_t* image)
{
    cmd_surface_free_t* cmd = display_cmds_push(image->disp, CMD_SURFACE_FREE, sizeof(cmd_surface_free_t));
    cmd->target = image->surface;
    display_cmds_flush(image->disp);

    free(image);
}

drawable_t* image_draw(image_t* image)
{
    return &image->draw;
}

uint64_t image_width(image_t* image)
{
    return image->width;
}

uint64_t image_height(image_t* image)
{
    return image->height;
}

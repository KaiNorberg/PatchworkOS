#include "internal.h"

#include <stdlib.h>

#define FBMP_MAGIC 0x706D6266

image_t* image_new_blank(display_t* disp, uint64_t width, uint64_t height)
{
    image_t* image = malloc(sizeof(image_t));
    if (image == NULL)
    {
        return NULL;
    }

    list_entry_init(&image->entry);
    image->draw.disp = disp;
    image->draw.stride = width;
    image->draw.buffer = malloc(width * height * sizeof(pixel_t));
    image->draw.contentRect = RECT_INIT_DIM(0, 0, width, height);
    image->draw.invalidRect = (rect_t){0};
    return image;
}

image_t* image_new(display_t* disp, const char* path)
{
    fd_t file = open(path);
    if (file == ERR)
    {
        return NULL;
    }

    struct 
    {
        uint32_t magic;
        uint32_t width;
        uint32_t height;
    } header;
    if (read(file, &header, sizeof(header)) == ERR)
    {
        close(file);
        return NULL;
    }

    uint64_t fileSize = seek(file, 0, SEEK_END);
    seek(file, 0, SEEK_SET);

    if (fileSize - sizeof(header) != header.width * header.height * sizeof(pixel_t) || header.magic != FBMP_MAGIC)
    {
        close(file);
        return NULL;
    }

    image_t* image = image_new_blank(disp, header.width, header.height);
    if (image == NULL)
    {
        close(file);
        return NULL;
    }

    read(file, image->draw.buffer, header.width * header.height * sizeof(pixel_t));
    close(file);
    return image;
}

void image_free(image_t* image)
{
    list_remove(&image->entry);
    free(image->draw.buffer);
    free(image);
}

drawable_t* image_draw(image_t* image)
{
    return &image->draw;
}

uint64_t image_width(image_t* image)
{
    return RECT_WIDTH(&image->draw.contentRect);
}

uint64_t image_height(image_t* image)
{
    return RECT_HEIGHT(&image->draw.contentRect);
}

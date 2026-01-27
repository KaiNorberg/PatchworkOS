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

    mtx_lock(&disp->mutex);
    list_push_back(&disp->images, &image->entry);
    mtx_unlock(&disp->mutex);
    return image;
}

image_t* image_new(display_t* disp, const char* path)
{
    fd_t file;
    status_t status = open(&file, path);
    if (IS_ERR(status))
    {
        return NULL;
    }

    struct
    {
        uint32_t magic;
        uint32_t width;
        uint32_t height;
    } header;
    status = read(file, &header, sizeof(header), NULL);
    if (IS_ERR(status))
    {
        close(file);
        return NULL;
    }

    uint64_t fileSize;
    seek(file, 0, SEEK_END, &fileSize);
    seek(file, sizeof(header), SEEK_SET, NULL);

    if (fileSize != header.width * header.height * sizeof(pixel_t) + sizeof(header) || header.magic != FBMP_MAGIC)
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

    status = read(file, image->draw.buffer, header.width * header.height * sizeof(pixel_t), NULL);
    if (IS_ERR(status))
    {
        image_free(image);
        close(file);
        return NULL;
    }
    close(file);

    return image;
}

void image_free(image_t* image)
{
    mtx_lock(&image->draw.disp->mutex);
    list_remove(&image->entry);
    mtx_unlock(&image->draw.disp->mutex);

    free(image->draw.buffer);
    free(image);
}

drawable_t* image_draw(image_t* image)
{
    return &image->draw;
}

void image_rect(image_t* image, rect_t* rect)
{
    *rect = image->draw.contentRect;
}

uint64_t image_width(image_t* image)
{
    return RECT_WIDTH(&image->draw.contentRect);
}

uint64_t image_height(image_t* image)
{
    return RECT_HEIGHT(&image->draw.contentRect);
}

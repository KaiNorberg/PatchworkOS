#include <libc/io.h>
#include <libgfx/gfx.h>
#include <libgui/gui.h>
#include <libgui/image.h>
#include <libgui/widget.h>
#include <png.h>
#include <stdlib.h>
#include <string.h>

#include "gui_internal.h"

typedef struct
{
    gui_widget_t widget;
    gui_image_t* image;
    gui_image_scale_t scale;
} gui_image_view_t;

status_t gui_image_load(const char* path, gui_image_t** out)
{
    if (path == NULL || out == NULL)
    {
        return ERR(USER, INVAL);
    }

    fd_t fd;
    status_t status = iowalk(FDCWD, FDROOT, path, &fd);
    if (IS_ERR(status))
    {
        return status;
    }

    size_t size;
    status = ioattr(fd, FILE_GET_SIZE, &size);
    if (IS_ERR(status))
    {
        iodrop(fd);
        return status;
    }

    uint8_t* buffer = malloc(size);
    if (buffer == NULL)
    {
        iodrop(fd);
        return ERR(USER, NOMEM);
    }

    status = ioread(fd, IOBUF(buffer, size), 0, NULL);
    if (IS_ERR(status))
    {
        free(buffer);
        iodrop(fd);
        return status;
    }

    iodrop(fd);

    png_image png;
    memset(&png, 0, sizeof(png));
    png.version = PNG_IMAGE_VERSION;

    if (!png_image_begin_read_from_memory(&png, buffer, size))
    {
        printf("gui_image_load: failed to read png %s\n", png.message);
        free(buffer);
        return ERR(USER, INVAL);
    }

    png.format = PNG_FORMAT_BGRA;

    gfx_pixel_t* pixels = malloc(PNG_IMAGE_SIZE(png));
    if (pixels == NULL)
    {
        png_image_free(&png);
        free(buffer);
        return ERR(USER, NOMEM);
    }

    if (!png_image_finish_read(&png, NULL, pixels, 0, NULL))
    {
        free(pixels);
        free(buffer);
        return ERR(USER, INVAL);
    }

    free(buffer);

    gui_image_t* img = malloc(sizeof(gui_image_t));
    if (img == NULL)
    {
        free(pixels);
        return ERR(USER, NOMEM);
    }

    img->width = png.width;
    img->height = png.height;
    img->pixels = pixels;
    img->gfx = GFX(pixels, png.width, png.height, png.width * sizeof(gfx_pixel_t));

    *out = img;
    return OK;
}

void gui_image_free(gui_image_t* image)
{
    if (image == NULL)
    {
        return;
    }

    free(image->pixels);
    free(image);
}

static status_t gui_image_view_draw(gui_widget_t* widget, gfx_t* gfx)
{
    if (widget == NULL || gfx == NULL)
    {
        return ERR(USER, INVAL);
    }

    gui_image_view_t* view = CONTAINER_OF(widget, gui_image_view_t, widget);
    if (view->image == NULL)
    {
        return OK;
    }

    gfx_rect_t rect = gui_widget_get_local_bounds(widget);
    gui_image_t* img = view->image;

    if (widget->gui->wallpaperScale == GUI_IMAGE_SCALE_STRETCH)
    {
        gfx_draw_scale_blit(gfx, &img->gfx, rect, img->gfx.clip, GFX_BLEND_ALPHA);
    }
    else if (widget->gui->wallpaperScale == GUI_IMAGE_SCALE_CENTER)
    {
        gfx_rect_t centered = GFX_RECT_FROM_CENTER(
            rect.left + GFX_RECT_WIDTH(rect) / 2,
            rect.top + GFX_RECT_HEIGHT(rect) / 2,
            img->width, img->height
        );
        gfx_draw_blit(gfx, &img->gfx, centered, img->gfx.clip);
    }
    else if (widget->gui->wallpaperScale == GUI_IMAGE_SCALE_FIT)
    {
        float scaleX = (float)GFX_RECT_WIDTH(rect) / img->width;
        float scaleY = (float)GFX_RECT_HEIGHT(rect) / img->height;
        float scale = MIN(scaleX, scaleY);

        int32_t newW = (int32_t)(img->width * scale);
        int32_t newH = (int32_t)(img->height * scale);

        gfx_rect_t fitRect = GFX_RECT_FROM_CENTER(
            rect.left + GFX_RECT_WIDTH(rect) / 2,
            rect.top + GFX_RECT_HEIGHT(rect) / 2,
            newW, newH
        );
        gfx_draw_scale_blit(gfx, &img->gfx, fitRect, img->gfx.clip, GFX_BLEND_ALPHA);
    }
    else if (widget->gui->wallpaperScale == GUI_IMAGE_SCALE_TILE)
    {
        for (int32_t ty = rect.top; ty < rect.bottom; ty += img->height)
        {
            for (int32_t tx = rect.left; tx < rect.right; tx += img->width)
            {
                gfx_rect_t tileRect = GFX_RECT(tx, ty, img->width, img->height);
                gfx_draw_blit(gfx, &img->gfx, tileRect, img->gfx.clip);
            }
        }
    }
    else
    {
        return ERR(USER, INVAL);
    }

    return OK;
}

static gui_class_t imageViewClass = {
    .size = sizeof(gui_image_view_t),
    .draw = gui_image_view_draw,
};

status_t gui_image_view_new(gui_widget_t* parent, gui_widget_id_t id, gfx_rect_t bounds, gui_image_t* image, gui_widget_t** out)
{
    gui_widget_t* widget;
    status_t status = gui_widget_new(&imageViewClass, parent, id, bounds, NULL, &widget);
    if (IS_ERR(status))
    {
        return status;
    }

    gui_image_view_t* view = CONTAINER_OF(widget, gui_image_view_t, widget);
    view->image = image;
    view->scale = GUI_IMAGE_SCALE_STRETCH;

    if (out != NULL)
    {
        *out = widget;
    }
    return OK;
}

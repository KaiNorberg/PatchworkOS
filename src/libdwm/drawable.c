#include "internal.h"

#include <stdlib.h>
#include <string.h>

void draw_rect(drawable_t* draw, const rect_t* rect, pixel_t pixel)
{
    rect_t fitRect = *rect;
    RECT_FIT(&fitRect, &draw->contentRect);

    for (int64_t y = fitRect.top; y < fitRect.bottom; y++)
    {
        memset32(&draw->buffer[fitRect.left + y * draw->stride], pixel, RECT_WIDTH(&fitRect));
    }

    draw_invalidate(draw, &fitRect);
}

void draw_edge(drawable_t* draw, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background)
{
    rect_t fitRect = *rect;
    RECT_FIT(&fitRect, &draw->contentRect);

    rect_t leftRect = (rect_t){
        .left = fitRect.left,
        .top = fitRect.top,
        .right = fitRect.left + width,
        .bottom = fitRect.bottom - width,
    };
    draw_rect(draw, &leftRect, foreground);

    rect_t topRect = (rect_t){
        .left = fitRect.left + width,
        .top = fitRect.top,
        .right = fitRect.right - width,
        .bottom = fitRect.top + width,
    };
    draw_rect(draw, &topRect, foreground);

    rect_t rightRect = (rect_t){
        .left = fitRect.right - width,
        .top = fitRect.top + width,
        .right = fitRect.right,
        .bottom = fitRect.bottom,
    };
    draw_rect(draw, &rightRect, background);

    rect_t bottomRect = (rect_t){
        .left = fitRect.left + width,
        .top = fitRect.bottom - width,
        .right = fitRect.right - width,
        .bottom = fitRect.bottom,
    };
    draw_rect(draw, &bottomRect, background);

    for (uint64_t y = 0; y < width; y++)
    {
        for (uint64_t x = 0; x < width; x++)
        {
            pixel_t color = x + y < width - 1 ? foreground : background;
            draw->buffer[(fitRect.right - width + x) + (fitRect.top + y) * draw->stride] = color;
            draw->buffer[(fitRect.left + x) + (fitRect.bottom - width + y) * draw->stride] = color;
        }
    }

    draw_invalidate(draw, &fitRect);
}

void draw_gradient(drawable_t* draw, const rect_t* rect, pixel_t start, pixel_t end, gradient_type_t type,
    bool addNoise)
{
    rect_t fitRect = *rect;
    RECT_FIT(&fitRect, &draw->contentRect);

    int64_t width = RECT_WIDTH(&fitRect);
    int64_t height = RECT_HEIGHT(&fitRect);

    for (int64_t y = fitRect.top; y < fitRect.bottom; y++)
    {
        for (int64_t x = fitRect.left; x < fitRect.right; x++)
        {
            int32_t factorNum;
            int32_t factorDenom;

            switch (type)
            {
            case GRADIENT_VERTICAL:
            {
                factorNum = (y - fitRect.top);
                factorDenom = height;
            }
            break;
            case GRADIENT_HORIZONTAL:
            {
                factorNum = (x - fitRect.left);
                factorDenom = width;
            }
            break;
            case GRADIENT_DIAGONAL:
            {
                factorNum = (x - fitRect.left) + (y - fitRect.top);
                factorDenom = width + height;
            }
            break;
            default:
            {
                factorNum = 0;
                factorDenom = 1;
            }
            break;
            }

            int32_t deltaRed = PIXEL_RED(end) - PIXEL_RED(start);
            int32_t deltaGreen = PIXEL_GREEN(end) - PIXEL_GREEN(start);
            int32_t deltaBlue = PIXEL_BLUE(end) - PIXEL_BLUE(start);

            int32_t red = PIXEL_RED(start) + ((factorNum * deltaRed) / factorDenom);
            int32_t green = PIXEL_GREEN(start) + ((factorNum * deltaGreen) / factorDenom);
            int32_t blue = PIXEL_BLUE(start) + ((factorNum * deltaBlue) / factorDenom);

            if (addNoise)
            {
                int32_t noiseRed = (rand() % 5) - 2;
                int32_t noiseGreen = (rand() % 5) - 2;
                int32_t noiseBlue = (rand() % 5) - 2;

                red += noiseRed;
                green += noiseGreen;
                blue += noiseBlue;

                red = CLAMP(0, 255, red);
                green = CLAMP(0, 255, green);
                blue = CLAMP(0, 255, blue);
            }

            pixel_t pixel = PIXEL_ARGB(255, red, green, blue);
            draw->buffer[x + y * draw->stride] = pixel;
        }
    }

    draw_invalidate(draw, &fitRect);
}

static void draw_psf(drawable_t* draw, const font_t* psf, const point_t* point, char chr, pixel_t foreground, pixel_t background)
{
    if (psf->glyphAmount < (uint32_t)chr)
    {
        return;
    }

    const uint8_t* glyph = psf->glyphs + chr * psf->glyphSize;

    if (PIXEL_ALPHA(foreground) == 0xFF && PIXEL_ALPHA(background) == 0xFF)
    {
        for (uint64_t y = 0; y < psf->height; y++)
        {
            for (uint64_t x = 0; x < psf->width; x++)
            {
                pixel_t pixel =
                    (glyph[y / psf->scale] & (0b10000000 >> (x / psf->scale))) != 0 ? foreground : background;
                draw->buffer[(point->x + x) + (point->y + y) * draw->stride] = pixel;
            }
        }
    }
    else
    {
        for (uint64_t y = 0; y < psf->height; y++)
        {
            for (uint64_t x = 0; x < psf->width; x++)
            {
                pixel_t pixel =
                    (glyph[y / psf->scale] & (0b10000000 >> (x / psf->scale))) != 0 ? foreground : background;
                PIXEL_BLEND(&draw->buffer[(point->x + x) + (point->y + y) * draw->stride], &pixel);
            }
        }
    }
}

void draw_string(drawable_t* draw, font_t* font, const point_t* point, pixel_t foreground, pixel_t background,
    const char* string, uint64_t length)
{
    if (font == NULL)
    {
        font = font_default(draw->disp);
    }

    rect_t textArea = RECT_INIT_DIM(point->x, point->y, font->width * length, font->height);
    if (!RECT_CONTAINS(&draw->contentRect, &textArea))
    {
        return;
    }

    point_t pos = *point;
    for (uint64_t i = 0; i < length; i++)
    {
        draw_psf(draw, font, &pos, string[i], foreground, background);
        pos.x += font->width;
    }

    draw_invalidate(draw, &textArea);
}

void draw_transfer(drawable_t* dest, drawable_t* src, const rect_t* destRect, const point_t* srcPoint)
{
    int64_t width = RECT_WIDTH(destRect);
    int64_t height = RECT_HEIGHT(destRect);

    if (width <= 0 || height <= 0)
    {
        return;
    }
    if (srcPoint->x < 0 || srcPoint->y < 0 || srcPoint->x + width > RECT_WIDTH(&src->contentRect) || srcPoint->y + height > RECT_HEIGHT(&src->contentRect))
    {
        return;
    }
    if (destRect->left < 0 || destRect->top < 0 || destRect->left + width > RECT_WIDTH(&dest->contentRect) ||
        destRect->top + height > RECT_HEIGHT(&dest->contentRect))
    {
        return;
    }

    if (dest == src)
    {
        for (int64_t y = 0; y < RECT_HEIGHT(destRect); y++)
        {
            memmove(&dest->buffer[destRect->left + (y + destRect->top) * dest->stride],
                &src->buffer[srcPoint->x + (y + srcPoint->y) * src->stride], RECT_WIDTH(destRect) * sizeof(pixel_t));
        }
    }
    else
    {
        for (int64_t y = 0; y < RECT_HEIGHT(destRect); y++)
        {
            memcpy(&dest->buffer[destRect->left + (y + destRect->top) * dest->stride],
                &src->buffer[srcPoint->x + (y + srcPoint->y) * src->stride], RECT_WIDTH(destRect) * sizeof(pixel_t));
        }
    }

    draw_invalidate(dest, destRect);
}

void draw_image(drawable_t* draw, image_t* image, const rect_t* destRect, const point_t* srcPoint)
{
    draw_transfer(draw, image_draw(image), destRect, srcPoint);
}

void draw_rim(drawable_t* draw, const rect_t* rect, uint64_t width, pixel_t pixel)
{
    rect_t leftRect = (rect_t){
        .left = rect->left,
        .top = rect->top + width - width / 2,
        .right = rect->left + width,
        .bottom = rect->bottom - width + width / 2,
    };
    draw_rect(draw, &leftRect, pixel);

    rect_t topRect = (rect_t){
        .left = rect->left + width - width / 2,
        .top = rect->top,
        .right = rect->right - width + width / 2,
        .bottom = rect->top + width,
    };
    draw_rect(draw, &topRect, pixel);

    rect_t rightRect = (rect_t){
        .left = rect->right - width,
        .top = rect->top + width - width / 2,
        .right = rect->right,
        .bottom = rect->bottom - width + width / 2,
    };
    draw_rect(draw, &rightRect, pixel);

    rect_t bottomRect = (rect_t){
        .left = rect->left + width - width / 2,
        .top = rect->bottom - width,
        .right = rect->right - width + width / 2,
        .bottom = rect->bottom,
    };
    draw_rect(draw, &bottomRect, pixel);
}

void draw_text(drawable_t* draw, const rect_t* rect, font_t* font, align_t xAlign, align_t yAlign, pixel_t foreground,
    pixel_t background, const char* text)
{
    if (text == NULL || *text == '\0')
    {
        return;
    }

    if (font == NULL)
    {
        font = font_default(draw->disp);
    }

    uint64_t maxWidth = RECT_WIDTH(rect);

    uint64_t textLen = strlen(text);
    uint64_t maxLen = maxWidth / font->width;

    uint64_t clampedLen = MIN(maxLen, textLen);
    uint64_t clampedWidth = clampedLen * font->width;

    point_t startPoint;
    switch (xAlign)
    {
    case ALIGN_CENTER:
    {
        startPoint.x = (rect->left + rect->right) / 2 - clampedWidth / 2;
    }
    break;
    case ALIGN_MAX:
    {
        startPoint.x = rect->right - clampedWidth;
    }
    break;
    case ALIGN_MIN:
    {
        startPoint.x = rect->left;
    }
    break;
    default:
    {
        return;
    }
    }

    switch (yAlign)
    {
    case ALIGN_CENTER:
    {
        startPoint.y = (rect->top + rect->bottom) / 2 - font->height / 2;
    }
    break;
    case ALIGN_MAX:
    {
        startPoint.y = MAX(rect->top, rect->bottom - (int64_t)font->height);
    }
    break;
    case ALIGN_MIN:
    {
        startPoint.y = rect->top;
    }
    break;
    default:
    {
        return;
    }
    }

    if (textLen > maxLen)
    {
        if (maxLen == 0)
        {
            return;
        }

        if (maxLen <= 3)
        {
            draw_string(draw, font, &startPoint, foreground, background, "...", maxLen);
            return;
        }

        draw_string(draw, font, &startPoint, foreground, background, text, maxLen - 3);
        startPoint.x += (maxLen - 3) * font->width;
        draw_string(draw, font, &startPoint, foreground, background, "...", 3);
        return;
    }

    draw_string(draw, font, &startPoint, foreground, background, text, textLen);
}

void draw_text_multiline(drawable_t* draw, const rect_t* rect, font_t* font, align_t xAlign, align_t yAlign,
    pixel_t foreground, pixel_t background, const char* text)
{
    if (text == NULL || *text == '\0')
    {
        return;
    }

    if (font == NULL)
    {
        font = font_default(draw->disp);
    }

    int64_t fontWidth = font_width(font);
    int64_t fontHeight = font_height(font);

    int64_t numLines = 1;
    int64_t maxLineWidth = 0;

    int64_t wordLength = 0;
    int64_t currentXPos = 0;
    const char* chr = text;
    while (true)
    {
        if (*chr == ' ' || *chr == '\0')
        {
            int64_t wordEndPos = currentXPos + wordLength * fontWidth;
            if (wordEndPos > RECT_WIDTH(rect))
            {
                numLines++;
                maxLineWidth = MAX(maxLineWidth, currentXPos);
                currentXPos = wordLength * fontWidth;
            }
            else
            {
                currentXPos = wordEndPos;
            }
            if (*chr == '\0')
            {
                maxLineWidth = MAX(maxLineWidth, currentXPos);
                break;
            }
            wordLength = 0;
            currentXPos += fontWidth;
        }
        else
        {
            wordLength++;
        }
        chr++;
    }

    point_t startPoint;
    switch (xAlign)
    {
    case ALIGN_CENTER:
    {
        startPoint.x = rect->left + (RECT_WIDTH(rect) - maxLineWidth) / 2;
    }
    break;
    case ALIGN_MAX:
    {
        startPoint.x = rect->right - maxLineWidth;
    }
    break;
    case ALIGN_MIN:
    {
        startPoint.x = rect->left;
    }
    break;
    default:
    {
        return;
    }
    }

    switch (yAlign)
    {
    case ALIGN_CENTER:
    {
        int64_t totalTextHeight = fontHeight * numLines;
        startPoint.y = rect->top + (RECT_HEIGHT(rect) - totalTextHeight) / 2;
    }
    break;
    case ALIGN_MAX:
    {
        startPoint.y = MAX(rect->top, rect->bottom - (int64_t)fontHeight * numLines);
    }
    break;
    case ALIGN_MIN:
    {
        startPoint.y = rect->top;
    }
    break;
    default:
    {
        return;
    }
    }

    chr = text;
    point_t currentPoint = startPoint;
    while (*chr != '\0')
    {
        const char* wordStart = chr;
        wordLength = 0;
        while (*chr != ' ' && *chr != '\0')
        {
            wordLength++;
            chr++;
        }

        int64_t wordWidth = wordLength * fontWidth;

        if (currentPoint.x + wordWidth > rect->right)
        {
            currentPoint.y += fontHeight;
            currentPoint.x = startPoint.x;
        }

        draw_string(draw, font, &currentPoint, foreground, background, wordStart, wordLength);
        currentPoint.x += wordWidth;

        if (*chr == ' ')
        {
            if (currentPoint.x + fontWidth > rect->right)
            {
                currentPoint.y += fontHeight;
            }
            else
            {
                draw_string(draw, font, &currentPoint, foreground, background, " ", 1);
                currentPoint.x += fontWidth;
            }
            chr++;
        }
    }
}

void draw_ridge(drawable_t* draw, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background)
{
    draw_edge(draw, rect, width / 2, background, foreground);

    rect_t innerRect = *rect;
    RECT_SHRINK(&innerRect, width / 2);
    draw_edge(draw, &innerRect, width / 2, foreground, background);
}

void draw_invalidate(drawable_t* draw, const rect_t* rect)
{
    if (rect == NULL)
    {
        draw->invalidRect = draw->contentRect;
        return;
    }

    if (RECT_AREA(&draw->invalidRect) == 0)
    {
        draw->invalidRect = *rect;
    }
    else
    {
        RECT_EXPAND_TO_CONTAIN(&draw->invalidRect, rect);
    }
}

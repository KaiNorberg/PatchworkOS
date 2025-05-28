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

void draw_frame(drawable_t* draw, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background)
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

void draw_outline(drawable_t* draw, const rect_t* rect, pixel_t pixel, uint32_t length, uint32_t width)
{
    rect_t fitRect = *rect;
    RECT_FIT(&fitRect, &draw->contentRect);

    if (RECT_WIDTH(&fitRect) <= 0 || RECT_HEIGHT(&fitRect) <= 0)
    {
        return;
    }

    uint32_t totalLength = length * 2;

    for (uint32_t w = 0; w < width; w++)
    {
        if (fitRect.top + w >= draw->contentRect.top && fitRect.top + w < draw->contentRect.bottom)
        {
            int64_t y = fitRect.top + w;
            for (int64_t x = fitRect.left; x < fitRect.right; x++)
            {
                uint32_t inPattern = (x - fitRect.left) % totalLength;
                if (inPattern < length)
                {
                    draw->buffer[x + y * draw->stride] = pixel;
                }
            }
        }

        if (fitRect.bottom - 1 - w >= draw->contentRect.top && fitRect.bottom - 1 - w < draw->contentRect.bottom &&
            RECT_HEIGHT(&fitRect) > 1 && fitRect.bottom - 1 - w > fitRect.top + w)
        {
            int64_t y = fitRect.bottom - 1 - w;
            for (int64_t x = fitRect.left; x < fitRect.right; x++)
            {
                uint32_t inPattern = (x - fitRect.left) % totalLength;
                if (inPattern < length)
                {
                    draw->buffer[x + y * draw->stride] = pixel;
                }
            }
        }
    }

    for (uint32_t w = 0; w < width; w++)
    {
        if (fitRect.left + w >= draw->contentRect.left && fitRect.left + w < draw->contentRect.right)
        {
            int64_t x = fitRect.left + w;
            for (int64_t y = fitRect.top + width; y < fitRect.bottom - width; y++)
            {
                uint32_t inPattern = (y - fitRect.top - width) % totalLength;
                if (inPattern < length)
                {
                    draw->buffer[x + y * draw->stride] = pixel;
                }
            }
        }

        if (fitRect.right - 1 - w >= draw->contentRect.left && fitRect.right - 1 - w < draw->contentRect.right &&
            RECT_WIDTH(&fitRect) > 1 && fitRect.right - 1 - w > fitRect.left + w)
        {
            int64_t x = fitRect.right - 1 - w;
            for (int64_t y = fitRect.top + width; y < fitRect.bottom - width; y++)
            {
                uint32_t inPattern = (y - fitRect.top - width) % totalLength;
                if (inPattern < length)
                {
                    draw->buffer[x + y * draw->stride] = pixel;
                }
            }
        }
    }

    draw_invalidate(draw, &fitRect);
}

void draw_gradient(drawable_t* draw, const rect_t* rect, pixel_t start, pixel_t end, direction_t direction,
    bool shouldAddNoise)
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

            switch (direction)
            {
            case DIRECTION_VERTICAL:
            {
                factorNum = (y - fitRect.top);
                factorDenom = height;
            }
            break;
            case DIRECTION_HORIZONTAL:
            {
                factorNum = (x - fitRect.left);
                factorDenom = width;
            }
            break;
            case DIRECTION_DIAGONAL:
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

            if (shouldAddNoise)
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

static void draw_grf_char(drawable_t* draw, const font_t* font, const point_t* point, uint8_t chr, pixel_t pixel)
{
    uint32_t offset = font->grf.glyphOffsets[chr];
    if (offset == GRF_NONE)
    {
        // TODO: Implement some sort of error char, empty box?
        return;
    }
    grf_glyph_t* glyph = (grf_glyph_t*)(&font->grf.buffer[offset]);

    int32_t baselineY = point->y + font->grf.ascender;

    for (uint16_t y = 0; y < glyph->height; y++)
    {
        for (uint16_t x = 0; x < glyph->width; x++)
        {
            uint8_t gray = glyph->buffer[y * glyph->width + x];

            if (gray > 0)
            {
                int32_t targetX = point->x + glyph->bearingX + x;
                int32_t targetY = baselineY - glyph->bearingY + y;

                if (targetX < 0 || targetY < 0 || targetX >= RECT_WIDTH(&draw->contentRect) ||
                    targetY >= RECT_HEIGHT(&draw->contentRect))
                {
                    continue;
                }

                pixel_t output = PIXEL_ARGB(gray, PIXEL_RED(pixel), PIXEL_GREEN(pixel), PIXEL_BLUE(pixel));
                PIXEL_BLEND(&draw->buffer[targetX + targetY * draw->stride], &output);
            }
        }
    }
}

void draw_string(drawable_t* draw, const font_t* font, const point_t* point, pixel_t pixel, const char* string,
    uint64_t length)
{
    if (font == NULL)
    {
        font = font_default(draw->disp);
    }

    uint64_t width = font_width(font, string, length);
    int32_t visualTextHeight = font->grf.ascender - font->grf.descender;
    rect_t textArea = RECT_INIT_DIM(point->x, point->y, width, visualTextHeight);

    point_t pos = *point;
    for (uint64_t i = 0; i < length; i++)
    {
        uint32_t offset = font->grf.glyphOffsets[(uint8_t)string[i]];
        if (offset == GRF_NONE)
        {
            // TODO: Implement some sort of error char, empty box?
            continue;
        }
        grf_glyph_t* glyph = (grf_glyph_t*)(&font->grf.buffer[offset]);

        draw_grf_char(draw, font, &pos, string[i], pixel);
        pos.x += glyph->advanceX;
        if (i != length - 1)
        {
            pos.x += font_kerning_offset(font, string[i], string[i + 1]);
        }
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
    if (srcPoint->x < 0 || srcPoint->y < 0 || srcPoint->x + width > RECT_WIDTH(&src->contentRect) ||
        srcPoint->y + height > RECT_HEIGHT(&src->contentRect))
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
        for (int64_t y = 0; y < height; y++)
        {
            memmove(&dest->buffer[destRect->left + (y + destRect->top) * dest->stride],
                &src->buffer[srcPoint->x + (y + srcPoint->y) * src->stride], width * sizeof(pixel_t));
        }
    }
    else
    {
        for (int64_t y = 0; y < height; y++)
        {
            memcpy(&dest->buffer[destRect->left + (y + destRect->top) * dest->stride],
                &src->buffer[srcPoint->x + (y + srcPoint->y) * src->stride], width * sizeof(pixel_t));
        }
    }

    draw_invalidate(dest, destRect);
}

void draw_image(drawable_t* draw, image_t* image, const rect_t* destRect, const point_t* srcPoint)
{
    draw_transfer(draw, image_draw(image), destRect, srcPoint);
}

void draw_transfer_blend(drawable_t* dest, drawable_t* src, const rect_t* destRect, const point_t* srcPoint)
{
    int64_t width = RECT_WIDTH(destRect);
    int64_t height = RECT_HEIGHT(destRect);

    if (width <= 0 || height <= 0)
    {
        return;
    }
    if (srcPoint->x < 0 || srcPoint->y < 0 || srcPoint->x + width > RECT_WIDTH(&src->contentRect) ||
        srcPoint->y + height > RECT_HEIGHT(&src->contentRect))
    {
        return;
    }
    if (destRect->left < 0 || destRect->top < 0 || destRect->left + width > RECT_WIDTH(&dest->contentRect) ||
        destRect->top + height > RECT_HEIGHT(&dest->contentRect))
    {
        return;
    }

    for (int64_t y = 0; y < height; y++)
    {
        for (int64_t x = 0; x < width; x++)
        {
            pixel_t srcPixel = src->buffer[(srcPoint->x + x) + (srcPoint->y + y) * src->stride];
            pixel_t* destPixel = &dest->buffer[(destRect->left + x) + (destRect->top + y) * dest->stride];
            PIXEL_BLEND(destPixel, &srcPixel);
        }
    }

    draw_invalidate(dest, destRect);
}

void draw_image_blend(drawable_t* draw, image_t* image, const rect_t* destRect, const point_t* srcPoint)
{
    draw_transfer_blend(draw, image_draw(image), destRect, srcPoint);
}

void draw_bezel(drawable_t* draw, const rect_t* rect, uint64_t width, pixel_t pixel)
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

static void draw_calculate_aligned_text_pos(const rect_t* rect, const font_t* font, const char* string, uint64_t length,
    align_t xAlign, align_t yAlign, point_t* aligned)
{
    uint64_t width = font_width(font, string, length);
    int32_t visualTextHeight = font->grf.ascender - font->grf.descender;

    switch (xAlign)
    {
    case ALIGN_MIN:
        aligned->x = rect->left;
        break;
    case ALIGN_CENTER:
        aligned->x = MAX(rect->left + (RECT_WIDTH(rect) / 2) - (width / 2), rect->left);
        break;
    case ALIGN_MAX:
        aligned->x = MAX(rect->left + RECT_WIDTH(rect) - width, rect->left);
        break;
    default:
        aligned->x = rect->left;
        break;
    }

    switch (yAlign)
    {
    case ALIGN_MIN:
        aligned->y = rect->top;
        break;
    case ALIGN_CENTER:
        aligned->y = rect->top + (RECT_HEIGHT(rect) / 2) - (visualTextHeight / 2);
        break;
    case ALIGN_MAX:
        aligned->y = rect->top + RECT_HEIGHT(rect) - visualTextHeight;
        break;
    default:
        aligned->y = rect->top;
        break;
    }
}

void draw_text(drawable_t* draw, const rect_t* rect, const font_t* font, align_t xAlign, align_t yAlign, pixel_t pixel,
    const char* text)
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
    uint64_t originalTextLen = strlen(text);
    uint64_t textWidth = font_width(font, text, originalTextLen);

    if (textWidth <= maxWidth)
    {
        point_t startPoint;
        draw_calculate_aligned_text_pos(rect, font, text, originalTextLen, xAlign, yAlign, &startPoint);
        draw_string(draw, font, &startPoint, pixel, text, originalTextLen);
    }
    else
    {
        const char* ellipsis = "...";
        uint64_t ellipsisWidth = font_width(font, ellipsis, 3);

        const char* drawText = text;
        uint64_t drawTextLen = 0;
        const char* drawEllipsis = NULL;

        if (ellipsisWidth <= maxWidth)
        {
            uint64_t currentContentWidth = 0;
            uint64_t fittedOriginalLen = 0;

            for (uint64_t i = 0; i < originalTextLen; ++i)
            {
                uint64_t charWidth = font_width(font, text + i, 1);
                if (currentContentWidth + charWidth + ellipsisWidth <= maxWidth)
                {
                    currentContentWidth += charWidth;
                    fittedOriginalLen++;
                }
                else
                {
                    break;
                }
            }

            drawText = text;
            drawTextLen = fittedOriginalLen;
            drawEllipsis = ellipsis;
        }
        else
        {
            uint64_t currentContentWidth = 0;
            uint64_t fittedEllipsisLen = 0;

            for (uint64_t i = 0; i < 3; ++i)
            {
                uint64_t charWidth = font_width(font, ellipsis + i, 1);
                if (currentContentWidth + charWidth <= maxWidth)
                {
                    currentContentWidth += charWidth;
                    fittedEllipsisLen++;
                }
                else
                {
                    break;
                }
            }

            drawText = ellipsis;
            drawTextLen = fittedEllipsisLen;
            drawEllipsis = NULL;
        }

        point_t startPoint;
        draw_calculate_aligned_text_pos(rect, font, drawText, drawTextLen + 3, xAlign, yAlign, &startPoint);
        draw_string(draw, font, &startPoint, pixel, drawText, drawTextLen);

        if (drawEllipsis != NULL)
        {
            point_t ellipsisStartPoint = {.x = startPoint.x + font_width(font, drawText, drawTextLen),
                .y = startPoint.y};
            draw_string(draw, font, &ellipsisStartPoint, pixel, drawEllipsis, 3);
        }
    }
}

void draw_text_multiline(drawable_t* draw, const rect_t* rect, const font_t* font, align_t xAlign, align_t yAlign,
    pixel_t pixel, const char* text)
{
    if (text == NULL || *text == '\0')
    {
        return;
    }

    if (font == NULL)
    {
        font = font_default(draw->disp);
    }

    rect_t fitRect = *rect;
    RECT_FIT(&fitRect, &draw->contentRect);

    int32_t lineHeight = font->grf.ascender - font->grf.descender;
    int32_t maxWidth = RECT_WIDTH(&fitRect);

    uint64_t lineCount = 0;
    const char* textPtr = text;

    while (*textPtr != '\0')
    {
        const char* lineStart = textPtr;
        const char* lineEnd = textPtr;
        const char* lastSpace = NULL;
        int32_t currentWidth = 0;

        while (*textPtr != '\0' && *textPtr != '\n')
        {
            if (*textPtr == ' ')
            {
                lastSpace = textPtr;
            }

            uint32_t offset = font->grf.glyphOffsets[(uint8_t)*textPtr];
            if (offset != GRF_NONE)
            {
                grf_glyph_t* glyph = (grf_glyph_t*)(&font->grf.buffer[offset]);
                currentWidth += glyph->advanceX;
                if (*(textPtr + 1) != '\0' && *(textPtr + 1) != '\n')
                {
                    currentWidth += font_kerning_offset(font, *textPtr, *(textPtr + 1));
                }

                if (currentWidth > maxWidth && lastSpace != NULL)
                {
                    lineEnd = lastSpace;
                    break;
                }
            }

            textPtr++;
        }

        if (*textPtr == '\n' || *textPtr == '\0')
        {
            lineEnd = textPtr;
        }
        else if (lastSpace == NULL)
        {
            lineEnd = textPtr;
        }

        lineCount++;

        if (*textPtr == '\n')
        {
            textPtr++;
        }
        else if (*textPtr != '\0')
        {
            textPtr = lineEnd + 1;
        }
    }

    int32_t totalTextHeight = lineCount * lineHeight;

    int32_t startY;
    switch (yAlign)
    {
    case ALIGN_MIN:
        startY = fitRect.top;
        break;
    case ALIGN_CENTER:
        startY = fitRect.top + (RECT_HEIGHT(&fitRect) / 2) - (totalTextHeight / 2);
        break;
    case ALIGN_MAX:
        startY = fitRect.top + RECT_HEIGHT(&fitRect) - totalTextHeight;
        break;
    default:
        startY = fitRect.top;
        break;
    }

    textPtr = text;
    int32_t currentY = startY;

    while (*textPtr != '\0' && currentY + lineHeight <= fitRect.bottom)
    {
        const char* lineStart = textPtr;
        const char* lineEnd = textPtr;
        const char* lastSpace = NULL;
        int32_t currentWidth = 0;

        while (*textPtr != '\0' && *textPtr != '\n')
        {
            if (*textPtr == ' ')
            {
                lastSpace = textPtr;
            }

            uint32_t offset = font->grf.glyphOffsets[(uint8_t)*textPtr];
            if (offset != GRF_NONE)
            {
                grf_glyph_t* glyph = (grf_glyph_t*)(&font->grf.buffer[offset]);
                currentWidth += glyph->advanceX;
                if (*(textPtr + 1) != '\0' && *(textPtr + 1) != '\n')
                {
                    currentWidth += font_kerning_offset(font, *textPtr, *(textPtr + 1));
                }

                if (currentWidth > maxWidth && lastSpace != NULL)
                {
                    lineEnd = lastSpace;
                    break;
                }
            }

            textPtr++;
        }

        if (*textPtr == '\n' || *textPtr == '\0')
        {
            lineEnd = textPtr;
        }
        else if (lastSpace == NULL)
        {
            lineEnd = textPtr;
        }

        uint64_t lineLength = lineEnd - lineStart;
        uint64_t lineWidth = font_width(font, lineStart, lineLength);

        int32_t lineX;
        switch (xAlign)
        {
        case ALIGN_MIN:
            lineX = fitRect.left;
            break;
        case ALIGN_CENTER:
            lineX = MAX(fitRect.left + (RECT_WIDTH(&fitRect) / 2) - (lineWidth / 2), fitRect.left);
            break;
        case ALIGN_MAX:
            lineX = MAX(fitRect.left + RECT_WIDTH(&fitRect) - lineWidth, fitRect.left);
            break;
        default:
            lineX = fitRect.left;
            break;
        }

        point_t linePos = {.x = lineX, .y = currentY};
        draw_string(draw, font, &linePos, pixel, lineStart, lineLength);

        currentY += lineHeight;

        if (*textPtr == '\n')
        {
            textPtr++;
        }
        else if (*textPtr != '\0')
        {
            textPtr = lineEnd + 1;
        }
    }

    draw_invalidate(draw, &fitRect);
}

void draw_ridge(drawable_t* draw, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background)
{
    draw_frame(draw, rect, width / 2, background, foreground);

    rect_t innerRect = *rect;
    RECT_SHRINK(&innerRect, width / 2);
    draw_frame(draw, &innerRect, width / 2, foreground, background);
}

void draw_separator(drawable_t* draw, const rect_t* rect, pixel_t highlight, pixel_t shadow, direction_t direction)
{
    rect_t fitRect = *rect;
    RECT_FIT(&fitRect, &draw->contentRect);

    switch (direction)
    {
    case DIRECTION_HORIZONTAL:
    {
        int64_t width = RECT_WIDTH(&fitRect);

        rect_t leftRect = {.left = fitRect.left,
            .top = fitRect.top,
            .right = fitRect.left + width / 2,
            .bottom = fitRect.bottom};
        rect_t rightRect = {.left = fitRect.left + width / 2,
            .top = fitRect.top,
            .right = fitRect.right,
            .bottom = fitRect.bottom};

        draw_rect(draw, &leftRect, highlight);
        draw_rect(draw, &rightRect, shadow);
    }
    break;
    case DIRECTION_VERTICAL:
    {
        int64_t height = RECT_HEIGHT(&fitRect);

        rect_t topRect = {.left = fitRect.left,
            .top = fitRect.top,
            .right = fitRect.left,
            .bottom = fitRect.top + height / 2};
        rect_t bottomRect = {.left = fitRect.left,
            .top = fitRect.top + height / 2,
            .right = fitRect.right,
            .bottom = fitRect.bottom};

        draw_rect(draw, &topRect, highlight);
        draw_rect(draw, &bottomRect, shadow);
    }
    break;
    default:
    {
    }
    }

    draw_invalidate(draw, &fitRect);
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

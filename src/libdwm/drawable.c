#include "internal.h"

#include <stdlib.h>
#include <string.h>

static void draw_rect_to_global(drawable_t* draw, rect_t* dest, const rect_t* src)
{
    *dest = (rect_t){
        .left = draw->drawArea.left + src->left,
        .top = draw->drawArea.top + src->top,
        .right = draw->drawArea.left + src->right,
        .bottom = draw->drawArea.top + src->bottom,
    };
}

static void draw_point_to_global(drawable_t* draw, point_t* dest, const point_t* src)
{
    *dest = (point_t){
        .x = draw->drawArea.left + src->x,
        .y = draw->drawArea.top + src->y,
    };
}

void draw_rect(drawable_t* draw, const rect_t* rect, pixel_t pixel)
{
    cmd_draw_rect_t cmd;
    CMD_INIT(&cmd, CMD_DRAW_RECT, sizeof(cmd));
    cmd.target = draw->surface;
    draw_rect_to_global(draw, &cmd.rect, rect);
    cmd.pixel = pixel;

    display_cmds_push(draw->disp, &cmd.header);

    draw_invalidate(draw, rect);
}

void draw_edge(drawable_t* draw, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background)
{
    cmd_draw_edge_t cmd;
    CMD_INIT(&cmd, CMD_DRAW_EDGE, sizeof(cmd));
    cmd.target = draw->surface;
    draw_rect_to_global(draw, &cmd.rect, rect);
    cmd.width = width;
    cmd.foreground = foreground;
    cmd.background = background;

    display_cmds_push(draw->disp, &cmd.header);

    draw_invalidate(draw, rect);
}

void draw_gradient(drawable_t* draw, const rect_t* rect, pixel_t start, pixel_t end, gradient_type_t type,
    bool addNoise)
{
    cmd_draw_gradient_t cmd;
    CMD_INIT(&cmd, CMD_DRAW_GRADIENT, sizeof(cmd));
    cmd.target = draw->surface;
    draw_rect_to_global(draw, &cmd.rect, rect);
    cmd.start = start;
    cmd.end = end;
    cmd.type = type;
    cmd.addNoise = addNoise;

    display_cmds_push(draw->disp, &cmd.header);

    draw_invalidate(draw, rect);
}

void draw_string(drawable_t* draw, font_t* font, const point_t* point, pixel_t foreground, pixel_t background,
    const char* string, uint64_t length)
{
    uint8_t buffer[sizeof(cmd_draw_string_t) + MAX_PATH];

    cmd_draw_string_t* cmd;
    if (length >= MAX_PATH) // If string is small stack allocate memory
    {
        cmd = malloc(sizeof(cmd_draw_string_t) + length);
    }
    else
    {
        cmd = (void*)buffer;
    }

    if (font == NULL)
    {
        font = font_default(draw->disp);
    }

    CMD_INIT(cmd, CMD_DRAW_STRING, sizeof(cmd_draw_string_t));
    cmd->target = draw->surface;
    cmd->fontId = font->id;
    draw_point_to_global(draw, &cmd->point, point);
    cmd->foreground = foreground;
    cmd->background = background;
    cmd->length = length;
    memcpy(cmd->string, string, length);
    cmd->header.size += length;
    display_cmds_push(draw->disp, &cmd->header);

    if (length >= MAX_PATH)
    {
        free(cmd);
    }

    rect_t rect = RECT_INIT_DIM(point->x, point->y, font->width * length, font->height);
    draw_invalidate(draw, &rect);
}

void draw_transfer(drawable_t* dest, drawable_t* src, const rect_t* destRect, const point_t* srcPoint)
{
    cmd_draw_transfer_t cmd;
    CMD_INIT(&cmd, CMD_DRAW_TRANSFER, sizeof(cmd));
    cmd.dest = dest->surface;
    cmd.src = src->surface;
    draw_rect_to_global(dest, &cmd.destRect, destRect);
    draw_point_to_global(src, &cmd.srcPoint, srcPoint);
    display_cmds_push(dest->disp, &cmd.header);

    draw_invalidate(dest, destRect);
    rect_t srcRect = RECT_INIT_DIM(srcPoint->x, srcPoint->y, RECT_WIDTH(destRect), RECT_HEIGHT(destRect));
    draw_invalidate(src, &srcRect);
}

void draw_buffer(drawable_t* draw, pixel_t* buffer, uint64_t index, uint64_t length)
{
    uint64_t cmdSize = sizeof(cmd_draw_buffer_t) + length * sizeof(pixel_t);

    cmd_draw_buffer_t* cmd = malloc(cmdSize);
    CMD_INIT(cmd, CMD_DRAW_BUFFER, cmdSize);
    cmd->target = draw->surface;
    cmd->index = index;
    cmd->length = length;
    memcpy(cmd->buffer, buffer, length * sizeof(pixel_t));
    display_cmds_push(draw->disp, &cmd->header);
    free(cmd);
}

void draw_image(drawable_t* draw, image_t* image, const rect_t *destRect, const point_t *srcPoint)
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

void draw_text(drawable_t* draw, const rect_t* rect, font_t* font, align_t xAlign, align_t yAlign,
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
    if (RECT_AREA(&draw->invalidRect) == 0)
    {
        draw->invalidRect = *rect;
    }
    else
    {
        RECT_EXPAND_TO_CONTAIN(&draw->invalidRect, rect);
    }
}

#include <libgfx/gfx.h>
#include <math.h>

void gfx_verts_circle(float* vertices, size_t count, float centerX, float centerY, float radius, float start, float end)
{
    if (vertices == NULL || count < 3)
    {
        return;
    }

    if (end < start)
    {
        return;
    }

    if (end - start > 2.0F * M_PI)
    {
        end = start + 2.0F * M_PI;
    }

    if (start == 0.0f && fabsf(end - 2.0F * (float)M_PI) < 0.0001f)
    {
        float step = (end - start) / (float)count;
        for (size_t i = 0; i < count; i++)
        {
            float angle = start + (float)i * step;
            GFX_VERTS_SET_X(vertices, i, centerX + roundf(cosf(angle) * radius));
            GFX_VERTS_SET_Y(vertices, i, centerY + roundf(sinf(angle) * radius));
        }
        return;
    }

    GFX_VERTS_SET_X(vertices, 0, centerX);
    GFX_VERTS_SET_Y(vertices, 0, centerY);

    float step = (end - start) / (float)(count - 2);
    for (size_t i = 1; i < count - 1; i++)
    {
        float angle = start + (float)(i - 1) * step;
        GFX_VERTS_SET_X(vertices, i, centerX + roundf(cosf(angle) * radius));
        GFX_VERTS_SET_Y(vertices, i, centerY + roundf(sinf(angle) * radius));
    }

    GFX_VERTS_SET_X(vertices, count - 1, centerX + roundf(cosf(end) * radius));
    GFX_VERTS_SET_Y(vertices, count - 1, centerY + roundf(sinf(end) * radius));
}

void gfx_verts_rotate(float* vertices, size_t count, float angle, float centerX, float centerY)
{
    if (vertices == NULL || count == 0)
    {
        return;
    }

    float cosAngle = cos(angle);
    float sinAngle = sin(angle);

    for (uint64_t i = 0; i < count; i++)
    {
        float translatedX = GFX_VERTS_GET_X(vertices, i) - centerX;
        float translatedY = GFX_VERTS_GET_Y(vertices, i) - centerY;

        int64_t rotatedX = (int64_t)round((translatedX * cosAngle) - (translatedY * sinAngle));
        int64_t rotatedY = (int64_t)round((translatedX * sinAngle) + (translatedY * cosAngle));

        GFX_VERTS_SET_X(vertices, i, rotatedX + centerX);
        GFX_VERTS_SET_Y(vertices, i, rotatedY + centerY);
    }
}

void gfx_verts_scale(float* vertices, size_t count, float scaleX, float scaleY, float centerX, float centerY)
{
    if (vertices == NULL || count == 0)
    {
        return;
    }

    for (uint64_t i = 0; i < count; i++)
    {
        float translatedX = GFX_VERTS_GET_X(vertices, i) - centerX;
        float translatedY = GFX_VERTS_GET_Y(vertices, i) - centerY;

        int64_t scaledX = (int64_t)round(translatedX * scaleX);
        int64_t scaledY = (int64_t)round(translatedY * scaleY);

        GFX_VERTS_SET_X(vertices, i, scaledX + centerX);
        GFX_VERTS_SET_Y(vertices, i, scaledY + centerY);
    }
}

void gfx_verts_translate(float* vertices, size_t count, float offsetX, float offsetY)
{
    if (vertices == NULL || count == 0)
    {
        return;
    }

    for (uint64_t i = 0; i < count; i++)
    {
        GFX_VERTS_SET_X(vertices, i, GFX_VERTS_GET_X(vertices, i) + offsetX);
        GFX_VERTS_SET_Y(vertices, i, GFX_VERTS_GET_Y(vertices, i) + offsetY);
    }
}

bool gfx_verts_contains(const float* vertices, size_t count, float x, float y)
{
    if (vertices == NULL || count == 0)
    {
        return false;
    }

    int32_t winding = 0;
    for (uint64_t i = 0; i < count; i++)
    {
        float x1 = GFX_VERTS_GET_X(vertices, i);
        float y1 = GFX_VERTS_GET_Y(vertices, i);
        float x2 = GFX_VERTS_GET_X(vertices, (i + 1) % count);
        float y2 = GFX_VERTS_GET_Y(vertices, (i + 1) % count);

        if (y1 <= y)
        {
            if (y2 > y && (x2 - x1) * (y - y1) - (x - x1) * (y2 - y1) > 0.0)
            {
                winding++;
            }
        }
        else
        {
            if (y2 <= y && (x2 - x1) * (y - y1) - (x - x1) * (y2 - y1) < 0.0)
            {
                winding--;
            }
        }
    }

    return winding != 0;
}

void gfx_verts_bounds(const float* vertices, size_t count, gfx_rect_t* bounds)
{
    if (vertices == NULL || count == 0 || bounds == NULL)
    {
        return;
    }

    int32_t minX = GFX_VERTS_GET_X(vertices, 0);
    int32_t maxX = GFX_VERTS_GET_X(vertices, 0);
    int32_t minY = GFX_VERTS_GET_Y(vertices, 0);
    int32_t maxY = GFX_VERTS_GET_Y(vertices, 0);

    for (size_t i = 1; i < count; i++)
    {
        int32_t x = GFX_VERTS_GET_X(vertices, i);
        int32_t y = GFX_VERTS_GET_Y(vertices, i);

        if (x < minX)
        {
            minX = x;
        }
        if (x > maxX)
        {
            maxX = x;
        }
        if (y < minY)
        {
            minY = y;
        }
        if (y > maxY)
        {
            maxY = y;
        }
    }

    bounds->left = minX;
    bounds->top = minY;
    bounds->right = maxX + 1;
    bounds->bottom = maxY + 1;
}
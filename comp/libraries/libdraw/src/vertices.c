#include <libdraw/draw.h>
#include <libdraw/vertices.h>
#include <math.h>

void vertices_circle(float* vertices, size_t count, float centerX, float centerY, float radius, float start, float end)
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
 
    if (start == 0.0f && fabsf(end - 2.0F * M_PI) < 0.0001f)
    {
        float step = (end - start) / (float)count;
        for (size_t i = 0; i < count; i++)
        {
            float angle = start + (float)i * step;
            VERTICES_SET_X(vertices, i, centerX + roundf(cosf(angle) * radius));
            VERTICES_SET_Y(vertices, i, centerY + roundf(sinf(angle) * radius));
        }
        return;
    }

    VERTICES_SET_X(vertices, 0, centerX);
    VERTICES_SET_Y(vertices, 0, centerY);

    float step = (end - start) / (float)(count - 2);
    for (size_t i = 1; i < count - 1; i++)
    {
        float angle = start + (float)(i - 1) * step;
        VERTICES_SET_X(vertices, i, centerX + roundf(cosf(angle) * radius));
        VERTICES_SET_Y(vertices, i, centerY + roundf(sinf(angle) * radius));
    }

    VERTICES_SET_X(vertices, count - 1, centerX + roundf(cosf(end) * radius));
    VERTICES_SET_Y(vertices, count - 1, centerY + roundf(sinf(end) * radius));
}

void vertices_rotate(float* vertices, size_t count, float angle, float centerX, float centerY)
{
    if (vertices == NULL || count == 0)
    {
        return;
    }

    float cosAngle = cos(angle);
    float sinAngle = sin(angle);

    for (uint64_t i = 0; i < count; i++)
    {
        float translatedX = VERTICES_GET_X(vertices, i) - centerX;
        float translatedY = VERTICES_GET_Y(vertices, i) - centerY;

        int64_t rotatedX = (int64_t)round((translatedX * cosAngle) - (translatedY * sinAngle));
        int64_t rotatedY = (int64_t)round((translatedX * sinAngle) + (translatedY * cosAngle));

        VERTICES_SET_X(vertices, i, rotatedX + centerX);
        VERTICES_SET_Y(vertices, i, rotatedY + centerY);
    }
}

void vertices_scale(float* vertices, size_t count, float scaleX, float scaleY, float centerX, float centerY)
{
    if (vertices == NULL || count == 0)
    {
        return;
    }

    for (uint64_t i = 0; i < count; i++)
    {
        float translatedX = VERTICES_GET_X(vertices, i) - centerX;
        float translatedY = VERTICES_GET_Y(vertices, i) - centerY;

        int64_t scaledX = (int64_t)round(translatedX * scaleX);
        int64_t scaledY = (int64_t)round(translatedY * scaleY);

        VERTICES_SET_X(vertices, i, scaledX + centerX);
        VERTICES_SET_Y(vertices, i, scaledY + centerY);
    }
}

void vertices_translate(float* vertices, size_t count, float offsetX, float offsetY)
{
    if (vertices == NULL || count == 0)
    {
        return;
    }

    for (uint64_t i = 0; i < count; i++)
    {
        VERTICES_SET_X(vertices, i, VERTICES_GET_X(vertices, i) + offsetX);
        VERTICES_SET_Y(vertices, i, VERTICES_GET_Y(vertices, i) + offsetY);
    }
}

bool vertices_contains(const float* vertices, size_t count, float x, float y)
{
    if (vertices == NULL || count == 0)
    {
        return false;
    }

    int32_t winding = 0;
    for (uint64_t i = 0; i < count; i++)
    {
        float x1 = VERTICES_GET_X(vertices, i);
        float y1 = VERTICES_GET_Y(vertices, i);
        float x2 = VERTICES_GET_X(vertices, (i + 1) % count);
        float y2 = VERTICES_GET_Y(vertices, (i + 1) % count);

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

void vertices_bounds(const float* vertices, size_t count, rect_t* bounds)
{
    if (vertices == NULL || count == 0 || bounds == NULL)
    {
        return;
    }

    int32_t minX = VERTICES_GET_X(vertices, 0);
    int32_t maxX = VERTICES_GET_X(vertices, 0);
    int32_t minY = VERTICES_GET_Y(vertices, 0);
    int32_t maxY = VERTICES_GET_Y(vertices, 0);

    for (size_t i = 1; i < count; i++)
    {
        int32_t x = VERTICES_GET_X(vertices, i);
        int32_t y = VERTICES_GET_Y(vertices, i);

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
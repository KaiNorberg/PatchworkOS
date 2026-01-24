#include <math.h>
#include <patchwork/polygon.h>

void polygon_rotate(point_t* points, uint64_t pointCount, double angle, point_t center)
{
    if (points == NULL || pointCount == 0)
    {
        return;
    }

    double cosAngle = cos(angle);
    double sinAngle = sin(angle);

    for (uint64_t i = 0; i < pointCount; i++)
    {
        double translatedX = points[i].x - center.x;
        double translatedY = points[i].y - center.y;

        int64_t rotatedX = (int64_t)round(translatedX * cosAngle - translatedY * sinAngle);
        int64_t rotatedY = (int64_t)round(translatedX * sinAngle + translatedY * cosAngle);

        points[i].x = rotatedX + center.x;
        points[i].y = rotatedY + center.y;
    }
}

bool polygon_contains(double px, double py, const point_t* points, uint64_t pointCount)
{
    int winding = 0;

    for (uint64_t i = 0; i < pointCount; i++)
    {
        point_t p1 = points[i];
        point_t p2 = points[(i + 1) % pointCount];

        if (p1.y <= py)
        {
            if (p2.y > py)
            {
                double vt = (py - p1.y) / (double)(p2.y - p1.y);
                if (px < p1.x + vt * (p2.x - p1.x))
                {
                    winding++;
                }
            }
        }
        else
        {
            if (p2.y <= py)
            {
                double vt = (py - p1.y) / (double)(p2.y - p1.y);
                if (px < p1.x + vt * (p2.x - p1.x))
                {
                    winding--;
                }
            }
        }
    }

    return winding != 0;
}

#ifndef DWM_SURFACE_H
#define DWM_SURFACE_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    SURFACE_WINDOW, // Window surface drawn within the client area of the screen, supports overlapping.

    SURFACE_PANEL, // Panel surface that defines the drawable client area for window surfaces. Always rendered on top of
                   // other surfaces except cursors.

    SURFACE_CURSOR, // Mouse surface for rendering the mouse cursor. Always rendered on top of everything else.

    SURFACE_WALL, // Wallpaper surface representing the desktop wallpaper. Always rendered below everything else.

    SURFACE_HIDDEN, // Off screen surface for rendering or storing graphics not displayed on screen, for example images.

    SURFACE_FULLSCREEN, // This one is a bit weird. Fullscreen surface that takes complete control of the display.
                        // Will always be the focused surface and prevents the dwm from rendering to the
                        // screen allowing a program to draw directly via the framebuffer devices, for example sys:/fb0.

    SURFACE_TYPE_AMOUNT
} surface_type_t;

typedef uint64_t surface_id_t;
#define SURFACE_ID_NONE (UINT64_MAX)

#if defined(__cplusplus)
}
#endif

#endif

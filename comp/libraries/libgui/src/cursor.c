#include <_libc/MAX_PATH.h>
#include <_libc/clock_t.h>
#include <libc/defs.h>
#include <libc/io.h>
#include <libgui/cursor.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* gui_cursor_to_string(gui_cursor_t cursor)
{
    static const char* const cursorStrings[] = {
        [GUI_CURSOR_NONE] = "default",
        [GUI_CURSOR_ALIAS] = "alias",
        [GUI_CURSOR_ALL_SCROLL] = "all-scroll",
        [GUI_CURSOR_ARROW] = "arrow",
        [GUI_CURSOR_BD_DOUBLE_ARROW] = "bd_double_arrow",
        [GUI_CURSOR_BOTTOM_LEFT_CORNER] = "bottom_left_corner",
        [GUI_CURSOR_BOTTOM_RIGHT_CORNER] = "bottom_right_corner",
        [GUI_CURSOR_BOTTOM_SIDE] = "bottom_side",
        [GUI_CURSOR_BOTTOM_TEE] = "bottom_tee",
        [GUI_CURSOR_CELL] = "cell",
        [GUI_CURSOR_CENTER_PTR] = "center_ptr",
        [GUI_CURSOR_CIRCLE] = "circle",
        [GUI_CURSOR_CLOSEDHAND] = "closedhand",
        [GUI_CURSOR_COLOR_PICKER] = "color-picker",
        [GUI_CURSOR_COL_RESIZE] = "col-resize",
        [GUI_CURSOR_CONTEXT_MENU] = "context-menu",
        [GUI_CURSOR_COPY] = "copy",
        [GUI_CURSOR_CROSS] = "cross",
        [GUI_CURSOR_CROSSED_CIRCLE] = "crossed_circle",
        [GUI_CURSOR_CROSSHAIR] = "crosshair",
        [GUI_CURSOR_CROSS_REVERSE] = "cross_reverse",
        [GUI_CURSOR_DEFAULT] = "default",
        [GUI_CURSOR_DIAMOND_CROSS] = "diamond_cross",
        [GUI_CURSOR_DND_ASK] = "dnd-ask",
        [GUI_CURSOR_DND_COPY] = "dnd-copy",
        [GUI_CURSOR_DND_LINK] = "dnd-link",
        [GUI_CURSOR_DND_MOVE] = "dnd-move",
        [GUI_CURSOR_DND_NO_DROP] = "dnd_no_drop",
        [GUI_CURSOR_DND_NONE] = "dnd-none",
        [GUI_CURSOR_DOTBOX] = "dotbox",
        [GUI_CURSOR_DOT_BOX_MASK] = "dot_box_mask",
        [GUI_CURSOR_DOUBLE_ARROW] = "double_arrow",
        [GUI_CURSOR_DOWN_ARROW] = "down-arrow",
        [GUI_CURSOR_DRAFT] = "draft",
        [GUI_CURSOR_DRAFT_LARGE] = "draft_large",
        [GUI_CURSOR_DRAFT_SMALL] = "draft_small",
        [GUI_CURSOR_DRAPED_BOX] = "draped_box",
        [GUI_CURSOR_E_RESIZE] = "e-resize",
        [GUI_CURSOR_EW_RESIZE] = "ew-resize",
        [GUI_CURSOR_FD_DOUBLE_ARROW] = "fd_double_arrow",
        [GUI_CURSOR_FLEUR] = "fleur",
        [GUI_CURSOR_FORBIDDEN] = "forbidden",
        [GUI_CURSOR_GRAB] = "grab",
        [GUI_CURSOR_GRABBING] = "grabbing",
        [GUI_CURSOR_HAND1] = "hand1",
        [GUI_CURSOR_HAND2] = "hand2",
        [GUI_CURSOR_H_DOUBLE_ARROW] = "h_double_arrow",
        [GUI_CURSOR_HELP] = "help",
        [GUI_CURSOR_IBEAM] = "ibeam",
        [GUI_CURSOR_ICON] = "icon",
        [GUI_CURSOR_LEFT_ARROW] = "left-arrow",
        [GUI_CURSOR_LEFT_PTR] = "left_ptr",
        [GUI_CURSOR_LEFT_PTR_HELP] = "left_ptr_help",
        [GUI_CURSOR_LEFT_PTR_WATCH] = "left_ptr_watch",
        [GUI_CURSOR_LEFT_SIDE] = "left_side",
        [GUI_CURSOR_LEFT_TEE] = "left_tee",
        [GUI_CURSOR_LINK] = "link",
        [GUI_CURSOR_LL_ANGLE] = "ll_angle",
        [GUI_CURSOR_LR_ANGLE] = "lr_angle",
        [GUI_CURSOR_MOVE] = "move",
        [GUI_CURSOR_NE_RESIZE] = "ne-resize",
        [GUI_CURSOR_NESW_RESIZE] = "nesw-resize",
        [GUI_CURSOR_NO_DROP] = "no-drop",
        [GUI_CURSOR_NOT_ALLOWED] = "not-allowed",
        [GUI_CURSOR_N_RESIZE] = "n-resize",
        [GUI_CURSOR_NS_RESIZE] = "ns-resize",
        [GUI_CURSOR_NW_RESIZE] = "nw-resize",
        [GUI_CURSOR_NWSE_RESIZE] = "nwse-resize",
        [GUI_CURSOR_OPENHAND] = "openhand",
        [GUI_CURSOR_PENCIL] = "pencil",
        [GUI_CURSOR_PIRATE] = "pirate",
        [GUI_CURSOR_PLUS] = "plus",
        [GUI_CURSOR_POINTER] = "pointer",
        [GUI_CURSOR_POINTER_MOVE] = "pointer-move",
        [GUI_CURSOR_POINTING_HAND] = "pointing_hand",
        [GUI_CURSOR_PROGRESS] = "progress",
        [GUI_CURSOR_QUESTION_ARROW] = "question_arrow",
        [GUI_CURSOR_RIGHT_ARROW] = "right-arrow",
        [GUI_CURSOR_RIGHT_PTR] = "right_ptr",
        [GUI_CURSOR_RIGHT_SIDE] = "right_side",
        [GUI_CURSOR_RIGHT_TEE] = "right_tee",
        [GUI_CURSOR_ROW_RESIZE] = "row-resize",
        [GUI_CURSOR_SB_DOWN_ARROW] = "sb_down_arrow",
        [GUI_CURSOR_SB_H_DOUBLE_ARROW] = "sb_h_double_arrow",
        [GUI_CURSOR_SB_LEFT_ARROW] = "sb_left_arrow",
        [GUI_CURSOR_SB_RIGHT_ARROW] = "sb_right_arrow",
        [GUI_CURSOR_SB_UP_ARROW] = "sb_up_arrow",
        [GUI_CURSOR_SB_V_DOUBLE_ARROW] = "sb_v_double_arrow",
        [GUI_CURSOR_SE_RESIZE] = "se-resize",
        [GUI_CURSOR_SIZE_ALL] = "size_all",
        [GUI_CURSOR_SIZE_BDIAG] = "size_bdiag",
        [GUI_CURSOR_SIZE_FDIAG] = "size_fdiag",
        [GUI_CURSOR_SIZE_HOR] = "size_hor",
        [GUI_CURSOR_SIZE_VER] = "size_ver",
        [GUI_CURSOR_SPLIT_H] = "split_h",
        [GUI_CURSOR_SPLIT_V] = "split_v",
        [GUI_CURSOR_S_RESIZE] = "s-resize",
        [GUI_CURSOR_SW_RESIZE] = "sw-resize",
        [GUI_CURSOR_TARGET] = "target",
        [GUI_CURSOR_TCROSS] = "tcross",
        [GUI_CURSOR_TEXT] = "text",
        [GUI_CURSOR_TOP_LEFT_ARROW] = "top_left_arrow",
        [GUI_CURSOR_TOP_LEFT_CORNER] = "top_left_corner",
        [GUI_CURSOR_TOP_RIGHT_CORNER] = "top_right_corner",
        [GUI_CURSOR_TOP_SIDE] = "top_side",
        [GUI_CURSOR_TOP_TEE] = "top_tee",
        [GUI_CURSOR_UL_ANGLE] = "ul_angle",
        [GUI_CURSOR_UP_ARROW] = "up-arrow",
        [GUI_CURSOR_UR_ANGLE] = "ur_angle",
        [GUI_CURSOR_V_DOUBLE_ARROW] = "v_double_arrow",
        [GUI_CURSOR_VERTICAL_TEXT] = "vertical-text",
        [GUI_CURSOR_WAIT] = "wait",
        [GUI_CURSOR_WATCH] = "watch",
        [GUI_CURSOR_WAYLAND_CURSOR] = "wayland-cursor",
        [GUI_CURSOR_WHATS_THIS] = "whats_this",
        [GUI_CURSOR_W_RESIZE] = "w-resize",
        [GUI_CURSOR_X_CURSOR] = "x-cursor",
        [GUI_CURSOR_XTERM] = "xterm",
        [GUI_CURSOR_ZOOM_IN] = "zoom-in",
        [GUI_CURSOR_ZOOM_OUT] = "zoom-out",
    };

    if (cursor < 0 || cursor >= ARRAY_SIZE(cursorStrings) || cursorStrings[cursor] == NULL)
    {
        return "default";
    }

    return cursorStrings[cursor];
}

typedef struct
{
    char magic[4];
    uint32_t headerSize;
    uint32_t version;
    uint32_t ntoc;
} gui_cursor_file_header_t;

typedef struct
{
    uint32_t type;
    uint32_t subtype;
    uint32_t position;
} gui_cursor_file_toc_entry_t;

typedef struct
{
    uint32_t headerSize;
    uint32_t type;
    uint32_t subtype;
    uint32_t version;
    uint32_t width;
    uint32_t height;
    uint32_t xhot;
    uint32_t yhot;
    uint32_t delay;
    uint32_t pixels[];
} gui_cursor_file_image_t;

status_t gui_cursor_file_load(const char* path, gui_cursor_file_t* out)
{
    if (path == NULL || out == NULL)
    {
        return ERR(USER, INVAL);
    }

    fd_t fd;
    status_t status = iowalk(FDCWD, FDROOT, path, &fd);
    if (IS_ERR(status))
    {
        printf("authman: failed to open cursor file %s %Y\n", path, status);

        return status;
    }

    gui_cursor_file_header_t header;
    size_t bytesRead;
    status = ioread(fd, IOBUF(&header, sizeof(header)), 0, &bytesRead);
    if (IS_ERR(status))
    {
        iodrop(fd);
        printf("authman: failed to read cursor file header %s %Y\n", path, status);
        return status;
    }

    if (bytesRead != sizeof(header))
    {
        printf("authman: failed to read cursor file header %s %Y\n", path, status);
        iodrop(fd);
        return ERR(USER, INVAL);
    }

    if (memcmp(header.magic, "Xcur", 4) != 0)
    {
        printf("authman: invalid cursor file header %s\n", path);
        iodrop(fd);
        return ERR(USER, INVAL);
    }

    gui_cursor_file_toc_entry_t* toc = calloc(header.ntoc, sizeof(gui_cursor_file_toc_entry_t));
    if (toc == NULL)
    {
        iodrop(fd);
        return ERR(USER, NOMEM);
    }

    status = ioread(fd, IOBUF(toc, sizeof(gui_cursor_file_toc_entry_t) * header.ntoc), sizeof(header), &bytesRead);
    if (IS_ERR(status))
    {
        printf("authman: failed to read cursor file toc %s %Y\n", path, status);
        free(toc);
        iodrop(fd);
        return status;
    }

    uint32_t imageCount = 0;
    for (size_t i = 0; i < header.ntoc; i++)
    {
        if (toc[i].type == 0xFFFD0002)
        {
            imageCount++;
        }
    }

    if (imageCount == 0)
    {
        printf("authman: no images in cursor file %s\n", path);
        free(toc);
        iodrop(fd);
        return ERR(USER, INVAL);
    }

    out->count = 0;
    out->images = NULL;

    uint32_t imgIndex = 0;
    for (size_t i = 0; i < header.ntoc && imgIndex < imageCount; i++)
    {
        if (toc[i].type != 0xFFFD0002)
        {
            continue;
        }

        gui_cursor_file_image_t imgHeader;
        status = ioread(fd, IOBUF(&imgHeader, sizeof(imgHeader)), toc[i].position, &bytesRead);
        if (IS_ERR(status) || bytesRead != sizeof(imgHeader))
        {
            continue;
        }

        size_t pixelDataSize = imgHeader.width * imgHeader.height * sizeof(uint32_t);
        gui_cursor_frame_t* frame = malloc(sizeof(gui_cursor_frame_t) + pixelDataSize);
        if (frame == NULL)
        {
            continue;
        }

        frame->xhot = imgHeader.xhot;
        frame->yhot = imgHeader.yhot;
        frame->delay = imgHeader.delay;

        status = ioread(fd, IOBUF(frame->pixels, pixelDataSize), toc[i].position + sizeof(imgHeader), &bytesRead);
        if (IS_ERR(status) || bytesRead != pixelDataSize)
        {
            free(frame);
            continue;
        }

        gui_cursor_image_t* image = NULL;
        for (size_t j = 0; j < out->count; j++)
        {
            if (out->images[j]->width == imgHeader.width && out->images[j]->height == imgHeader.height)
            {
                image = out->images[j];
                break;
            }
        }

        if (image == NULL)
        {
            gui_cursor_image_t** newImages = realloc(out->images, (out->count + 1) * sizeof(gui_cursor_image_t*));
            if (newImages == NULL)
            {
                free(frame);
                continue;
            }
            out->images = newImages;

            image = calloc(1, sizeof(gui_cursor_image_t));
            if (image == NULL)
            {
                free(frame);
                continue;
            }
            image->width = imgHeader.width;
            image->height = imgHeader.height;
            out->images[out->count++] = image;
        }

        gui_cursor_frame_t** newFrames = realloc(image->frames, (image->count + 1) * sizeof(gui_cursor_frame_t*));
        if (newFrames == NULL)
        {
            free(frame);
            continue;
        }
        image->frames = newFrames;
        image->frames[image->count++] = frame;

        imgIndex++;
    }

    free(toc);
    iodrop(fd);
    return OK;
}

typedef struct gui_cursor_theme
{
    gui_cursor_file_t files[GUI_CURSOR_MAX];
} gui_cursor_theme_t;

status_t gui_cursor_theme_load(gui_cursor_theme_t** out)
{
    if (out == NULL)
    {
        return ERR(USER, INVAL);
    }

    gui_cursor_theme_t* theme = calloc(1, sizeof(gui_cursor_theme_t));
    if (theme == NULL)
    {
        return ERR(USER, NOMEM);
    }

    uint64_t loadedCursors = 0;
    for (size_t i = 0; i < GUI_CURSOR_MAX; i++)
    {
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "/share/cursors/%s", gui_cursor_to_string(i));

        status_t status = gui_cursor_file_load(path, &theme->files[i]);
        if (IS_ERR(status))
        {
            if (IS_CODE(status, NOENT))
            {
                continue;
            }

            gui_cursor_theme_free(theme);
            return status;
        }

        loadedCursors++;
    }

    if (loadedCursors == 0)
    {
        gui_cursor_theme_free(theme);
        return ERR(USER, NOENT);
    }

    *out = theme;
    return OK;
}

void gui_cursor_theme_free(gui_cursor_theme_t* theme)
{
    if (theme == NULL)
    {
        return;
    }

    for (size_t i = 0; i < GUI_CURSOR_MAX; i++)
    {
        if (theme->files[i].images == NULL)
        {
            continue;
        }

        for (size_t j = 0; j < theme->files[i].count; j++)
        {
            gui_cursor_image_t* image = theme->files[i].images[j];
            if (image == NULL)
                continue;

            for (size_t k = 0; k < image->count; k++)
            {
                free(image->frames[k]);
            }
            free(image->frames);
            free(image);
        }

        free(theme->files[i].images);
    }

    free(theme);
}

gui_cursor_file_t* gui_cursor_theme_get_cursor(gui_cursor_theme_t* theme, gui_cursor_t cursor)
{
    if (theme == NULL || cursor < 0 || cursor >= GUI_CURSOR_MAX)
    {
        return NULL;
    }

    return &theme->files[cursor] != NULL ? &theme->files[cursor] : &theme->files[GUI_CURSOR_NONE];
}

#define GUI_CURSOR_MAX_SIZE 128

typedef struct gui_cursor_state
{
    gui_cursor_t type;
    gui_cursor_theme_t* theme;
    uint32_t desiredWidth;
    uint32_t desiredHeight;
    uint32_t currentFrame;
    clock_t lastFrame;
    gfx_t gfx;
    gfx_rect_t oldRect;
    gfx_pixel_t oldPixels[GUI_CURSOR_MAX_SIZE * GUI_CURSOR_MAX_SIZE];
} gui_cursor_state_t;

status_t gui_cursor_state_new(gui_cursor_theme_t* theme, uint32_t desiredWidth, uint32_t desiredHeight,
    gui_cursor_state_t** out)
{
    if (theme == NULL || out == NULL)
    {
        return ERR(USER, INVAL);
    }

    gui_cursor_state_t* state = calloc(1, sizeof(gui_cursor_state_t));
    if (state == NULL)
    {
        return ERR(USER, NOMEM);
    }

    state->theme = theme;
    state->type = GUI_CURSOR_DEFAULT;
    state->desiredWidth = desiredWidth;
    state->desiredHeight = desiredHeight;
    state->currentFrame = 0;
    state->lastFrame = 0;
    state->gfx =
        GFX(state->oldPixels, GUI_CURSOR_MAX_SIZE, GUI_CURSOR_MAX_SIZE, GUI_CURSOR_MAX_SIZE * sizeof(gfx_pixel_t));
    state->oldRect = GFX_RECT(0, 0, 0, 0);

    *out = state;
    return OK;
}

void gui_cursor_state_free(gui_cursor_state_t* state)
{
    if (state == NULL)
    {
        return;
    }

    free(state);
}

static gui_cursor_image_t* gui_cursor_state_get_image(gui_cursor_state_t* state)
{
    gui_cursor_file_t* file = &state->theme->files[state->type];
    if (file == NULL || file->count == 0)
    {
        return NULL;
    }

    gui_cursor_image_t* best = file->images[0];
    uint32_t bestDiff = UINT32_MAX;

    for (size_t i = 0; i < file->count; i++)
    {
        gui_cursor_image_t* img = file->images[i];
        if (img == NULL)
            continue;

        uint32_t diffX =
            img->width > state->desiredWidth ? img->width - state->desiredWidth : state->desiredWidth - img->width;
        uint32_t diffY = img->height > state->desiredHeight ? img->height - state->desiredHeight
                                                            : state->desiredHeight - img->height;
        uint32_t diff = diffX + diffY;

        if (diff < bestDiff)
        {
            best = img;
            bestDiff = diff;
        }
    }

    return best;
}

clock_t gui_cursor_state_next_frame(gui_cursor_state_t* state, clock_t now)
{
    if (state == NULL)
    {
        return CLOCKS_NEVER;
    }

    gui_cursor_image_t* image = gui_cursor_state_get_image(state);
    if (image == NULL || image->count <= 1)
    {
        return CLOCKS_NEVER;
    }

    gui_cursor_frame_t* frame = image->frames[state->currentFrame % image->count];
    if (frame == NULL)
    {
        return CLOCKS_NEVER;
    }
    
    clock_t delay = (clock_t)(frame->delay * CLOCKS_PER_MS);
    clock_t elapsed = now - state->lastFrame;
    if (elapsed >= delay)
    {
        return 0;
    }

    return delay - elapsed;
}

void gui_cursor_state_update(gui_cursor_state_t* state, gui_cursor_t cursor, clock_t now)
{
    if (state == NULL)
    {
        return;
    }

    if (state->type != cursor)
    {
        state->type = cursor;
        state->currentFrame = 0;
        state->lastFrame = now;
        return;
    }

    gui_cursor_image_t* image = gui_cursor_state_get_image(state);
    if (image == NULL || image->count <= 1)
    {
        return;
    }

    gui_cursor_frame_t* frame = image->frames[state->currentFrame % image->count];
    if (frame == NULL)
    {
        return;
    }

    if (now - state->lastFrame >= (clock_t)(frame->delay * CLOCKS_PER_MS))
    {
        state->currentFrame = (state->currentFrame + 1) % image->count;
        state->lastFrame = now;
    }
}

void gui_cursor_state_draw(gui_cursor_state_t* state, gfx_t* gfx, int32_t x, int32_t y)
{
    if (state == NULL || gfx == NULL)
    {
        return;
    }

    gui_cursor_image_t* image = gui_cursor_state_get_image(state);
    if (image == NULL || image->count == 0)
    {
        return;
    }

    gui_cursor_frame_t* frame = image->frames[state->currentFrame % image->count];
    if (frame == NULL)
    {
        return;
    }

    gfx_rect_t rect = GFX_RECT(x - frame->xhot, y - frame->yhot, image->width, image->height);
    gfx_t cursorGfx = GFX(frame->pixels, image->width, image->height, image->width * sizeof(uint32_t));

    gfx_draw_copy(&state->gfx, gfx, GFX_RECT(0, 0, image->width, image->height), rect);
    state->oldRect = rect;

    gfx_draw_blit(gfx, &cursorGfx, rect, GFX_RECT(0, 0, image->width, image->height));
}

void gui_cursor_state_clear(gui_cursor_state_t* state, gfx_t* gfx)
{
    if (state == NULL || gfx == NULL)
    {
        return;
    }

    gfx_rect_t localRect = GFX_RECT(0, 0, GFX_RECT_WIDTH(state->oldRect), GFX_RECT_HEIGHT(state->oldRect));
    gfx_draw_copy(gfx, &state->gfx, state->oldRect, localRect);
}

gfx_rect_t gui_cursor_state_get_bounds(gui_cursor_state_t* state)
{
    if (state == NULL)
    {
        return GFX_RECT(0, 0, 0, 0);
    }

    return state->oldRect;
}
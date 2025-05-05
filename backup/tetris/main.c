#include "sys/gfx.h"
#include "sys/kbd.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/proc.h>
#include <sys/win.h>

#define BLOCK_SIZE 32

#define FIELD_PADDING 10
#define FIELD_WIDTH 10
#define FIELD_HEIGHT 20
#define FIELD_LEFT (FIELD_PADDING)
#define FIELD_TOP (FIELD_PADDING)
#define FIELD_RIGHT (FIELD_PADDING + BLOCK_SIZE * FIELD_WIDTH)
#define FIELD_BOTTOM (FIELD_PADDING + BLOCK_SIZE * FIELD_HEIGHT)

#define SIDE_PANEL_PADDING 20
#define SIDE_PANEL_WIDTH 220
#define SIDE_PANEL_LEFT (FIELD_RIGHT + FIELD_PADDING)
#define SIDE_PANEL_TOP (FIELD_TOP)
#define SIDE_PANEL_RIGHT (SIDE_PANEL_LEFT + SIDE_PANEL_WIDTH - FIELD_PADDING)
#define SIDE_PANEL_BOTTOM (FIELD_BOTTOM)
#define SIDE_PANEL_TEXT_HEIGHT 42
#define SIDE_PANEL_LABEL_HEIGHT 42
#define SIDE_PANEL_FONT_SIZE 32
#define SIDE_PANEL_LABEL_PADDING 40

#define START_SCREEN_FONT_SIZE 64

#define WINDOW_WIDTH ((FIELD_WIDTH) * BLOCK_SIZE + FIELD_PADDING * 2 + SIDE_PANEL_WIDTH)
#define WINDOW_HEIGHT ((FIELD_HEIGHT) * BLOCK_SIZE + FIELD_PADDING * 2)

#define CURRENT_SCORE_WIDGET_ID 0
#define COMPLETE_LINES_WIDGET_ID 1
#define PLAYED_BLOCKS_WIDGET_ID 2

#define TICK_SPEED (SEC)
#define DROPPING_TICK_SPEED (SEC / 12)
#define CLEARING_LINES_TICK_SPEED (SEC / 15)
#define START_SCREEN_TICK_SPEED ((SEC / 4) * 3)

#define PIECE_AMOUNT 7
#define PIECE_WIDTH 4
#define PIECE_HEIGHT 4

typedef enum
{
    BLOCK_INVAL,
    BLOCK_NONE,
    BLOCK_CYAN,
    BLOCK_BLUE,
    BLOCK_ORANGE,
    BLOCK_YELLOW,
    BLOCK_GREEN,
    BLOCK_PURPLE,
    BLOCK_RED,
    BLOCK_CLEARING,
    BLOCK_OUTLINE,
} block_t;

typedef block_t piece_t[PIECE_HEIGHT][PIECE_WIDTH];

typedef enum
{
    PIECE_NONE,
    PIECE_CYAN,
    PIECE_BLUE,
    PIECE_ORANGE,
    PIECE_YELLOW,
    PIECE_GREEN,
    PIECE_PURPLE,
    PIECE_RED,
} piece_type_t;

static piece_t pieces[] = {
    [PIECE_CYAN] =
        {
            {BLOCK_NONE, BLOCK_NONE, BLOCK_NONE, BLOCK_NONE},
            {BLOCK_NONE, BLOCK_NONE, BLOCK_NONE, BLOCK_NONE},
            {BLOCK_CYAN, BLOCK_CYAN, BLOCK_CYAN, BLOCK_CYAN},
            {BLOCK_NONE, BLOCK_NONE, BLOCK_NONE, BLOCK_NONE},
        },
    [PIECE_BLUE] =
        {
            {BLOCK_NONE, BLOCK_NONE, BLOCK_NONE, BLOCK_NONE},
            {BLOCK_BLUE, BLOCK_NONE, BLOCK_NONE, BLOCK_NONE},
            {BLOCK_BLUE, BLOCK_BLUE, BLOCK_BLUE, BLOCK_NONE},
            {BLOCK_NONE, BLOCK_NONE, BLOCK_NONE, BLOCK_NONE},
        },
    [PIECE_ORANGE] =
        {
            {BLOCK_NONE, BLOCK_NONE, BLOCK_NONE, BLOCK_NONE},
            {BLOCK_NONE, BLOCK_NONE, BLOCK_ORANGE, BLOCK_NONE},
            {BLOCK_ORANGE, BLOCK_ORANGE, BLOCK_ORANGE, BLOCK_NONE},
            {BLOCK_NONE, BLOCK_NONE, BLOCK_NONE, BLOCK_NONE},
        },
    [PIECE_YELLOW] =
        {
            {BLOCK_NONE, BLOCK_NONE, BLOCK_NONE, BLOCK_NONE},
            {BLOCK_NONE, BLOCK_YELLOW, BLOCK_YELLOW, BLOCK_NONE},
            {BLOCK_NONE, BLOCK_YELLOW, BLOCK_YELLOW, BLOCK_NONE},
            {BLOCK_NONE, BLOCK_NONE, BLOCK_NONE, BLOCK_NONE},
        },
    [PIECE_GREEN] =
        {
            {BLOCK_NONE, BLOCK_NONE, BLOCK_NONE, BLOCK_NONE},
            {BLOCK_NONE, BLOCK_GREEN, BLOCK_GREEN, BLOCK_NONE},
            {BLOCK_GREEN, BLOCK_GREEN, BLOCK_NONE, BLOCK_NONE},
            {BLOCK_NONE, BLOCK_NONE, BLOCK_NONE, BLOCK_NONE},
        },
    [PIECE_PURPLE] =
        {
            {BLOCK_NONE, BLOCK_NONE, BLOCK_NONE, BLOCK_NONE},
            {BLOCK_NONE, BLOCK_PURPLE, BLOCK_NONE, BLOCK_NONE},
            {BLOCK_PURPLE, BLOCK_PURPLE, BLOCK_PURPLE, BLOCK_NONE},
            {BLOCK_NONE, BLOCK_NONE, BLOCK_NONE, BLOCK_NONE},
        },
    [PIECE_RED] =
        {
            {BLOCK_NONE, BLOCK_NONE, BLOCK_NONE, BLOCK_NONE},
            {BLOCK_RED, BLOCK_RED, BLOCK_NONE, BLOCK_NONE},
            {BLOCK_NONE, BLOCK_RED, BLOCK_RED, BLOCK_NONE},
            {BLOCK_NONE, BLOCK_NONE, BLOCK_NONE, BLOCK_NONE},
        },
};

static block_t field[FIELD_HEIGHT][FIELD_WIDTH];
static block_t oldField[FIELD_HEIGHT][FIELD_WIDTH];

static uint64_t currentScore;
static uint64_t completedLines;
static uint64_t playedBlocks;

static uint64_t oldCurrentScore;
static uint64_t oldCompletedLines;
static uint64_t oldPlayedBlocks;

static bool clearingLines;
static bool started;
static bool gameover;

static struct
{
    piece_t piece;
    int64_t x;
    int64_t y;
    bool dropping;
} currentPiece;

static const pixel_t normalColors[] = {
    [BLOCK_NONE] = PIXEL_ARGB(0xFF, 0x00, 0x00, 0x00),
    [BLOCK_CYAN] = PIXEL_ARGB(0xFF, 0x00, 0xE5, 0xFF),
    [BLOCK_BLUE] = PIXEL_ARGB(0xFF, 0x00, 0x55, 0xFF),
    [BLOCK_ORANGE] = PIXEL_ARGB(0xFF, 0xFF, 0x7A, 0x00),
    [BLOCK_YELLOW] = PIXEL_ARGB(0xFF, 0xFF, 0xE1, 0x00),
    [BLOCK_GREEN] = PIXEL_ARGB(0xFF, 0x00, 0xFF, 0x4D),
    [BLOCK_PURPLE] = PIXEL_ARGB(0xFF, 0xD2, 0x00, 0xFF),
    [BLOCK_RED] = PIXEL_ARGB(0xFF, 0xFF, 0x00, 0x55),
    [BLOCK_CLEARING] = PIXEL_ARGB(0xFF, 0xFF, 0xFF, 0xFF),
    [BLOCK_OUTLINE] = PIXEL_ARGB(0xFF, 0x00, 0x00, 0x00),
};
static const pixel_t highlightColors[] = {
    [BLOCK_NONE] = PIXEL_ARGB(0xFF, 0x00, 0x00, 0x00),
    [BLOCK_CYAN] = PIXEL_ARGB(0xFF, 0x98, 0xF5, 0xFF),
    [BLOCK_BLUE] = PIXEL_ARGB(0xFF, 0x98, 0xB9, 0xFF),
    [BLOCK_ORANGE] = PIXEL_ARGB(0xFF, 0xFF, 0xBF, 0x98),
    [BLOCK_YELLOW] = PIXEL_ARGB(0xFF, 0xFF, 0xF3, 0x98),
    [BLOCK_GREEN] = PIXEL_ARGB(0xFF, 0x98, 0xFF, 0xB3),
    [BLOCK_PURPLE] = PIXEL_ARGB(0xFF, 0xED, 0x98, 0xFF),
    [BLOCK_RED] = PIXEL_ARGB(0xFF, 0xFF, 0x98, 0xB9),
    [BLOCK_CLEARING] = PIXEL_ARGB(0xFF, 0xFF, 0xFF, 0xFF),
    [BLOCK_OUTLINE] = PIXEL_ARGB(0xFF, 0xEE, 0xEE, 0xEE),
};
static const pixel_t shadowColors[] = {
    [BLOCK_NONE] = PIXEL_ARGB(0xFF, 0x00, 0x00, 0x00),
    [BLOCK_CYAN] = PIXEL_ARGB(0xFF, 0x00, 0x7A, 0x8C),
    [BLOCK_BLUE] = PIXEL_ARGB(0xFF, 0x00, 0x2A, 0x8C),
    [BLOCK_ORANGE] = PIXEL_ARGB(0xFF, 0x8C, 0x46, 0x00),
    [BLOCK_YELLOW] = PIXEL_ARGB(0xFF, 0x8C, 0x7D, 0x00),
    [BLOCK_GREEN] = PIXEL_ARGB(0xFF, 0x00, 0x8C, 0x2A),
    [BLOCK_PURPLE] = PIXEL_ARGB(0xFF, 0x75, 0x00, 0x8C),
    [BLOCK_RED] = PIXEL_ARGB(0xFF, 0x8C, 0x00, 0x2A),
    [BLOCK_CLEARING] = PIXEL_ARGB(0xFF, 0xFF, 0xFF, 0xFF),
    [BLOCK_OUTLINE] = PIXEL_ARGB(0xFF, 0xEE, 0xEE, 0xEE),
};

static void current_piece_choose_new(void);
static void current_piece_clear(gfx_t* gfx);
static void current_piece_draw(gfx_t* gfx);

static void block_draw(gfx_t* gfx, block_t block, int64_t x, int64_t y)
{
    if (x < 0 || y < 0 || x >= FIELD_WIDTH || y >= FIELD_HEIGHT)
    {
        return;
    }

    rect_t rect = RECT_INIT_DIM(FIELD_LEFT + x * BLOCK_SIZE, FIELD_TOP + y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE);

    gfx_edge(gfx, &rect, windowTheme.edgeWidth, highlightColors[block], shadowColors[block]);
    RECT_SHRINK(&rect, windowTheme.edgeWidth);
    gfx_rect(gfx, &rect, normalColors[block]);
    RECT_SHRINK(&rect, 5);
    gfx_edge(gfx, &rect, windowTheme.edgeWidth, shadowColors[block], highlightColors[block]);
}

static void side_panel_draw(win_t* window, gfx_t* gfx)
{
    rect_t rect = RECT_INIT(SIDE_PANEL_LEFT, SIDE_PANEL_TOP, SIDE_PANEL_RIGHT, SIDE_PANEL_BOTTOM);

    gfx_ridge(gfx, &rect, windowTheme.ridgeWidth, windowTheme.highlight, windowTheme.shadow);

    rect_t textRect = rect;
    textRect.bottom = textRect.top + SIDE_PANEL_TEXT_HEIGHT;
    gfx_text(gfx, win_font(window), &textRect, GFX_CENTER, GFX_CENTER, SIDE_PANEL_FONT_SIZE, "Score", windowTheme.dark,
        windowTheme.background);

    textRect.top = textRect.bottom + SIDE_PANEL_LABEL_HEIGHT;
    textRect.bottom = textRect.top + SIDE_PANEL_TEXT_HEIGHT;
    gfx_text(gfx, win_font(window), &textRect, GFX_CENTER, GFX_CENTER, SIDE_PANEL_FONT_SIZE, "Lines", windowTheme.dark,
        windowTheme.background);

    textRect.top = textRect.bottom + SIDE_PANEL_LABEL_HEIGHT;
    textRect.bottom = textRect.top + SIDE_PANEL_TEXT_HEIGHT;
    gfx_text(gfx, win_font(window), &textRect, GFX_CENTER, GFX_CENTER, SIDE_PANEL_FONT_SIZE, "Pieces", windowTheme.dark,
        windowTheme.background);

    textRect.top = rect.bottom - SIDE_PANEL_FONT_SIZE * 7;
    textRect.bottom = rect.bottom;
    gfx_text(gfx, win_font(window), &textRect, GFX_CENTER, GFX_CENTER, SIDE_PANEL_FONT_SIZE, "  ASD - Move", windowTheme.dark,
        windowTheme.background);
    textRect.top += SIDE_PANEL_FONT_SIZE;
    textRect.bottom += SIDE_PANEL_FONT_SIZE;
    gfx_text(gfx, win_font(window), &textRect, GFX_CENTER, GFX_CENTER, SIDE_PANEL_FONT_SIZE, "SPACE - Drop", windowTheme.dark,
        windowTheme.background);
    textRect.top += SIDE_PANEL_FONT_SIZE;
    textRect.bottom += SIDE_PANEL_FONT_SIZE;
    gfx_text(gfx, win_font(window), &textRect, GFX_CENTER, GFX_CENTER, SIDE_PANEL_FONT_SIZE, "    R - Spin", windowTheme.dark,
        windowTheme.background);
}

static point_t piece_block_pos_in_field(int64_t pieceX, int64_t pieceY, int64_t blockX, int64_t blockY)
{
    return (point_t){.x = pieceX + blockX - PIECE_WIDTH / 2, .y = pieceY + blockY - PIECE_HEIGHT / 2};
}

static bool piece_out_of_bounds(const piece_t* piece, int64_t pieceX, int64_t pieceY)
{
    for (int64_t blockY = 0; blockY < PIECE_HEIGHT; blockY++)
    {
        for (int64_t blockX = 0; blockX < PIECE_WIDTH; blockX++)
        {
            if ((*piece)[blockY][blockX] == BLOCK_NONE)
            {
                continue;
            }

            point_t point = piece_block_pos_in_field(pieceX, pieceY, blockX, blockY);
            if (point.x < 0 || point.x >= FIELD_WIDTH || point.y >= FIELD_HEIGHT) // point.y < 0 check is left out on purpose
            {
                return true;
            }
        }
    }

    return false;
}

static void piece_clear(gfx_t* gfx, const piece_t* piece, uint64_t pieceX, uint64_t pieceY)
{
    for (int64_t blockY = 0; blockY < PIECE_HEIGHT; blockY++)
    {
        for (int64_t blockX = 0; blockX < PIECE_WIDTH; blockX++)
        {
            if ((*piece)[blockY][blockX] == BLOCK_NONE)
            {
                continue;
            }

            point_t point = piece_block_pos_in_field(pieceX, pieceY, blockX, blockY);
            block_draw(gfx, BLOCK_NONE, point.x, point.y);
        }
    }
}

static void piece_outline_draw(gfx_t* gfx, const piece_t* piece, uint64_t pieceX, uint64_t pieceY)
{
    for (int64_t blockY = 0; blockY < PIECE_HEIGHT; blockY++)
    {
        for (int64_t blockX = 0; blockX < PIECE_WIDTH; blockX++)
        {
            if ((*piece)[blockY][blockX] == BLOCK_NONE)
            {
                continue;
            }

            point_t point = piece_block_pos_in_field(pieceX, pieceY, blockX, blockY);
            block_draw(gfx, BLOCK_OUTLINE, point.x, point.y);
        }
    }
}

static void piece_draw(gfx_t* gfx, const piece_t* piece, uint64_t pieceX, uint64_t pieceY)
{
    for (int64_t blockY = 0; blockY < PIECE_HEIGHT; blockY++)
    {
        for (int64_t blockX = 0; blockX < PIECE_WIDTH; blockX++)
        {
            if ((*piece)[blockY][blockX] == BLOCK_NONE)
            {
                continue;
            }

            point_t point = piece_block_pos_in_field(pieceX, pieceY, blockX, blockY);
            block_draw(gfx, (*piece)[blockY][blockX], point.x, point.y);
        }
    }
}

static void piece_rotate(piece_t* piece)
{
    for (uint64_t i = 0; i < 2; i++)
    {
        for (uint64_t j = i; j < 4 - i - 1; j++)
        {
            block_t temp = (*piece)[i][j];
            (*piece)[i][j] = (*piece)[4 - 1 - j][i];
            (*piece)[4 - 1 - j][i] = (*piece)[4 - 1 - i][4 - 1 - j];
            (*piece)[4 - 1 - i][4 - 1 - j] = (*piece)[j][4 - 1 - i];
            (*piece)[j][4 - 1 - i] = temp;
        }
    }
}

static void field_edge_draw(gfx_t* gfx)
{
    rect_t fieldRect = RECT_INIT(FIELD_LEFT, FIELD_TOP, FIELD_RIGHT, FIELD_BOTTOM);
    RECT_EXPAND(&fieldRect, FIELD_PADDING);
    gfx_rim(gfx, &fieldRect, FIELD_PADDING - windowTheme.edgeWidth, windowTheme.background);
    RECT_SHRINK(&fieldRect, FIELD_PADDING - windowTheme.edgeWidth);
    gfx_edge(gfx, &fieldRect, windowTheme.edgeWidth, windowTheme.shadow, windowTheme.highlight);
}

static void field_draw(gfx_t* gfx)
{
    for (uint64_t y = 0; y < FIELD_HEIGHT; y++)
    {
        for (uint64_t x = 0; x < FIELD_WIDTH; x++)
        {
            if (field[y][x] == oldField[y][x])
            {
                continue;
            }
            oldField[y][x] = field[y][x];

            block_draw(gfx, field[y][x], x, y);
        }
    }
}

static bool field_collides(const piece_t* piece, int64_t pieceX, int64_t pieceY)
{
    for (int64_t blockY = 0; blockY < PIECE_HEIGHT; blockY++)
    {
        for (int64_t blockX = 0; blockX < PIECE_WIDTH; blockX++)
        {
            if ((*piece)[blockY][blockX] == BLOCK_NONE)
            {
                continue;
            }

            point_t point = piece_block_pos_in_field(pieceX, pieceY, blockX, blockY);
            if (point.x < 0 || point.x >= FIELD_WIDTH || point.y < 0 || point.y >= FIELD_HEIGHT)
            {
                continue;
            }
            if (field[point.y][point.x] != BLOCK_NONE)
            {
                return true;
            }
        }
    }

    return false;
}

static void field_add_piece(const piece_t* piece, int64_t pieceX, int64_t pieceY)
{
    for (uint64_t blockY = 0; blockY < PIECE_HEIGHT; blockY++)
    {
        for (uint64_t blockX = 0; blockX < PIECE_WIDTH; blockX++)
        {
            if ((*piece)[blockY][blockX] == BLOCK_NONE)
            {
                continue;
            }

            point_t point = piece_block_pos_in_field(pieceX, pieceY, blockX, blockY);
            field[point.y][point.x] = currentPiece.piece[blockY][blockX];
        }
    }
}

static void field_move_down(uint64_t line)
{
    for (int64_t y = line; y >= 1; y--)
    {
        memcpy(&field[y], &field[y - 1], sizeof(block_t) * FIELD_WIDTH);
    }

    for (uint64_t x = 0; x < FIELD_WIDTH; x++)
    {
        field[0][x] = BLOCK_NONE;
    }
}

static void field_clear_lines(gfx_t* gfx)
{
    current_piece_clear(gfx);
    bool done = true;
    for (uint64_t y = 0; y < FIELD_HEIGHT; y++)
    {
        int64_t x = 0;
        for (; x < FIELD_WIDTH / 2; x++)
        {
            if (field[y][x] != BLOCK_CLEARING)
            {
                break;
            }
        }

        if (x == 0)
        {
            continue;
        }

        field[y][x - 1] = BLOCK_NONE;
        field[y][FIELD_WIDTH - x] = BLOCK_NONE;

        if (x == 1)
        {
            field_move_down(y);
        }

        done = false;
    }

    if (!done)
    {
        field_draw(gfx);
    }
    else
    {
        clearingLines = false;
    }

    current_piece_draw(gfx);
}

static void field_check_for_lines(gfx_t* gfx)
{
    uint64_t foundLines = 0;
    for (uint64_t y = 0; y < FIELD_HEIGHT; y++)
    {
        bool completeLine = true;
        for (uint64_t x = 0; x < FIELD_WIDTH; x++)
        {
            if (field[y][x] == BLOCK_NONE)
            {
                completeLine = false;
                break;
            }
        }

        if (completeLine)
        {
            for (uint64_t x = 0; x < FIELD_WIDTH; x++)
            {
                field[y][x] = BLOCK_CLEARING;
            }
            clearingLines = true;
            completedLines++;
            foundLines++;
        }
    }

    switch (foundLines)
    {
    case 1:
    {
        currentScore += 40;
    }
    break;
    case 2:
    {
        currentScore += 100;
    }
    break;
    case 3:
    {
        currentScore += 300;
    }
    break;
    case 4:
    {
        currentScore += 1200;
    }
    break;
    }

    field_draw(gfx);
}

static void pause()
{
    clearingLines = false;

    for (int64_t y = 0; y < FIELD_HEIGHT; y++)
    {
        for (int64_t x = 0; x < FIELD_WIDTH; x++)
        {
            field[y][x] = BLOCK_NONE;
            oldField[y][x] = BLOCK_INVAL;
        }
    }

    started = false;
    gameover = false;
}

static void start()
{
    currentScore = 0;
    completedLines = 0;
    playedBlocks = 0;

    clearingLines = false;

    for (int64_t y = 0; y < FIELD_HEIGHT; y++)
    {
        for (int64_t x = 0; x < FIELD_WIDTH; x++)
        {
            field[y][x] = BLOCK_NONE;
            oldField[y][x] = BLOCK_INVAL;
        }
    }

    current_piece_choose_new();
    currentPiece.dropping = false;

    started = true;
    gameover = false;
}

static void current_piece_choose_new(void)
{
    memcpy(&currentPiece.piece, &pieces[rand() % PIECE_AMOUNT + 1], sizeof(piece_t));
    currentPiece.x = 5;
    currentPiece.y = 0;

    playedBlocks++;

    if (field_collides(&currentPiece.piece, currentPiece.x, currentPiece.y))
    {
        pause();
        gameover = true;
    }
}

static void current_piece_clear(gfx_t* gfx)
{
    int64_t outlineY = currentPiece.y;
    while (!piece_out_of_bounds(&currentPiece.piece, currentPiece.x, outlineY) &&
        !field_collides(&currentPiece.piece, currentPiece.x, outlineY))
    {
        outlineY++;
    }
    outlineY--;

    piece_clear(gfx, &currentPiece.piece, currentPiece.x, outlineY);
    piece_clear(gfx, &currentPiece.piece, currentPiece.x, currentPiece.y);
}

static void current_piece_draw(gfx_t* gfx)
{
    int64_t outlineY = currentPiece.y;
    while (!piece_out_of_bounds(&currentPiece.piece, currentPiece.x, outlineY) &&
        !field_collides(&currentPiece.piece, currentPiece.x, outlineY))
    {
        outlineY++;
    }
    outlineY--;

    piece_outline_draw(gfx, &currentPiece.piece, currentPiece.x, outlineY);
    piece_draw(gfx, &currentPiece.piece, currentPiece.x, currentPiece.y);
}

static void current_piece_update(gfx_t* gfx)
{
    if (piece_out_of_bounds(&currentPiece.piece, currentPiece.x, currentPiece.y + 1) ||
        field_collides(&currentPiece.piece, currentPiece.x, currentPiece.y + 1))
    {
        field_add_piece(&currentPiece.piece, currentPiece.x, currentPiece.y);
        current_piece_choose_new();
        current_piece_draw(gfx);
        field_check_for_lines(gfx);
    }
    else
    {
        current_piece_clear(gfx);
        currentPiece.y++;
        current_piece_draw(gfx);
    }
}

static void current_piece_move(gfx_t* gfx, keycode_t code)
{
    uint64_t newX = currentPiece.x + (code == KEY_D) - (code == KEY_A);

    if (piece_out_of_bounds(&currentPiece.piece, newX, currentPiece.y) ||
        field_collides(&currentPiece.piece, newX, currentPiece.y))
    {
        return;
    }

    current_piece_clear(gfx);
    currentPiece.x = newX;
    current_piece_draw(gfx);
}

static void current_piece_drop(gfx_t* gfx)
{
    current_piece_clear(gfx);

    while (!piece_out_of_bounds(&currentPiece.piece, currentPiece.x, currentPiece.y) &&
        !field_collides(&currentPiece.piece, currentPiece.x, currentPiece.y))
    {
        currentPiece.y++;
    }
    currentPiece.y--;

    current_piece_draw(gfx);
}

static void current_piece_rotate(gfx_t* gfx)
{
    piece_t rotatedPiece;
    memcpy(&rotatedPiece, &currentPiece.piece, sizeof(piece_t));
    piece_rotate(&rotatedPiece);

    if (piece_out_of_bounds(&rotatedPiece, currentPiece.x, currentPiece.y) ||
        field_collides(&rotatedPiece, currentPiece.x, currentPiece.y))
    {
        return;
    }

    current_piece_clear(gfx);
    memcpy(&currentPiece.piece, &rotatedPiece, sizeof(piece_t));
    current_piece_draw(gfx);
}

static void start_tetris_draw(win_t* window, gfx_t* gfx)
{
    rect_t rect = RECT_INIT((FIELD_RIGHT + FIELD_LEFT) / 2 - (START_SCREEN_FONT_SIZE / 2) * 3, FIELD_TOP,
        (FIELD_RIGHT + FIELD_LEFT) / 2 - (START_SCREEN_FONT_SIZE / 2) * 2, FIELD_TOP + (FIELD_BOTTOM - FIELD_TOP) / 2);

    gfx_text(gfx, win_font(window), &rect, GFX_CENTER, GFX_CENTER, START_SCREEN_FONT_SIZE, "T", normalColors[BLOCK_RED],
        windowTheme.dark);
    rect.left += (START_SCREEN_FONT_SIZE / 2) + 2;
    rect.right += (START_SCREEN_FONT_SIZE / 2) + 2;
    gfx_text(gfx, win_font(window), &rect, GFX_CENTER, GFX_CENTER, START_SCREEN_FONT_SIZE, "E", normalColors[BLOCK_ORANGE],
        windowTheme.dark);
    rect.left += (START_SCREEN_FONT_SIZE / 2) - 2;
    rect.right += (START_SCREEN_FONT_SIZE / 2) - 2;
    gfx_text(gfx, win_font(window), &rect, GFX_CENTER, GFX_CENTER, START_SCREEN_FONT_SIZE, "T", normalColors[BLOCK_YELLOW],
        windowTheme.dark);
    rect.left += (START_SCREEN_FONT_SIZE / 2) + 2;
    rect.right += (START_SCREEN_FONT_SIZE / 2) + 2;
    gfx_text(gfx, win_font(window), &rect, GFX_CENTER, GFX_CENTER, START_SCREEN_FONT_SIZE, "R", normalColors[BLOCK_GREEN],
        windowTheme.dark);
    rect.left += (START_SCREEN_FONT_SIZE / 2) - 2;
    rect.right += (START_SCREEN_FONT_SIZE / 2) - 2;
    gfx_text(gfx, win_font(window), &rect, GFX_CENTER, GFX_CENTER, START_SCREEN_FONT_SIZE, "I", normalColors[BLOCK_CYAN],
        windowTheme.dark);
    rect.left += (START_SCREEN_FONT_SIZE / 2);
    rect.right += (START_SCREEN_FONT_SIZE / 2);
    gfx_text(gfx, win_font(window), &rect, GFX_CENTER, GFX_CENTER, START_SCREEN_FONT_SIZE, "S", normalColors[BLOCK_BLUE],
        windowTheme.dark);
}

static void start_press_space_draw(win_t* window, gfx_t* gfx)
{
    static bool blink = false;

    rect_t rect = RECT_INIT(FIELD_LEFT, (FIELD_TOP + FIELD_BOTTOM) / 2, FIELD_RIGHT, FIELD_BOTTOM);
    gfx_text(gfx, win_font(window), &rect, GFX_CENTER, GFX_CENTER, START_SCREEN_FONT_SIZE / 2, "PRESS SPACE",
        blink ? windowTheme.bright : windowTheme.dark, windowTheme.dark);
    blink = !blink;
}

static uint64_t procedure(win_t* window, const msg_t* msg)
{
    switch (msg->type)
    {
    case LMSG_INIT:
    {
        srand(uptime());

        currentScore = 0;
        completedLines = 0;
        playedBlocks = 0;

        win_text_prop_t prop = {
            .height = SIDE_PANEL_FONT_SIZE,
            .foreground = windowTheme.bright,
            .background = windowTheme.dark,
            .xAlign = GFX_CENTER,
            .yAlign = GFX_CENTER,
        };

        rect_t textRect = RECT_INIT(SIDE_PANEL_LEFT + SIDE_PANEL_LABEL_PADDING, SIDE_PANEL_TOP + SIDE_PANEL_TEXT_HEIGHT,
            SIDE_PANEL_RIGHT - SIDE_PANEL_LABEL_PADDING, SIDE_PANEL_TOP + SIDE_PANEL_TEXT_HEIGHT + SIDE_PANEL_LABEL_HEIGHT);
        win_label_new(window, "000000", &textRect, CURRENT_SCORE_WIDGET_ID, &prop);

        textRect.top = textRect.bottom + SIDE_PANEL_LABEL_HEIGHT;
        textRect.bottom = textRect.top + SIDE_PANEL_TEXT_HEIGHT;
        win_label_new(window, "000000", &textRect, COMPLETE_LINES_WIDGET_ID, &prop);

        textRect.top = textRect.bottom + SIDE_PANEL_LABEL_HEIGHT;
        textRect.bottom = textRect.top + SIDE_PANEL_TEXT_HEIGHT;
        win_label_new(window, "000000", &textRect, PLAYED_BLOCKS_WIDGET_ID, &prop);

        pause();
    }
    break;
    case LMSG_REDRAW:
    {
        gfx_t gfx;
        win_draw_begin(window, &gfx);

        field_edge_draw(&gfx);
        field_draw(&gfx);
        side_panel_draw(window, &gfx);

        win_draw_end(window, &gfx);
        win_timer_set(window, 0);
    }
    break;
    case LMSG_TIMER:
    {
        if (!started)
        {
            gfx_t gfx;
            win_draw_begin(window, &gfx);
            start_tetris_draw(window, &gfx);
            start_press_space_draw(window, &gfx);
            win_draw_end(window, &gfx);
            win_timer_set(window, START_SCREEN_TICK_SPEED);
            break;
        }
        else if (clearingLines)
        {
            gfx_t gfx;
            win_draw_begin(window, &gfx);
            field_clear_lines(&gfx);
            win_draw_end(window, &gfx);
            win_timer_set(window, CLEARING_LINES_TICK_SPEED);
            break;
        }
        else if (currentPiece.dropping)
        {
            win_timer_set(window, DROPPING_TICK_SPEED);
        }
        else
        {
            win_timer_set(window, TICK_SPEED);
        }

        gfx_t gfx;
        win_draw_begin(window, &gfx);
        current_piece_update(&gfx);
        win_draw_end(window, &gfx);

        if (clearingLines || gameover)
        {
            gameover = false;
            win_timer_set(window, 0);
        }
    }
    break;
    case MSG_KBD:
    {
        msg_kbd_t* data = (msg_kbd_t*)msg->data;

        if (!started)
        {
            if (data->type == KBD_PRESS && data->code == KEY_SPACE)
            {
                start();
                win_send(window, LMSG_REDRAW, NULL, 0);
            }

            break;
        }
        else if (clearingLines)
        {
            currentPiece.dropping = false;
            break;
        }

        if (data->type == KBD_PRESS && (data->code == KEY_A || data->code == KEY_D))
        {
            gfx_t gfx;
            win_draw_begin(window, &gfx);
            current_piece_move(&gfx, data->code);
            win_draw_end(window, &gfx);
        }
        else if (data->type == KBD_PRESS && data->code == KEY_R)
        {
            gfx_t gfx;
            win_draw_begin(window, &gfx);
            current_piece_rotate(&gfx);
            win_draw_end(window, &gfx);
        }
        else if (data->type == KBD_PRESS && data->code == KEY_S)
        {
            win_timer_set(window, 0);
            currentPiece.dropping = true;
        }
        else if (data->type == KBD_PRESS && data->code == KEY_SPACE)
        {
            gfx_t gfx;
            win_draw_begin(window, &gfx);
            current_piece_drop(&gfx);
            win_draw_end(window, &gfx);
            win_timer_set(window, 0);
        }
        else if (data->type == KBD_RELEASE && data->code == KEY_S)
        {
            win_timer_set(window, TICK_SPEED);
            currentPiece.dropping = false;
        }
    }
    break;
    }

    if (currentScore != oldCurrentScore)
    {
        char buffer[7];
        sprintf(buffer, "%06d", currentScore);
        win_widget_name_set(win_widget(window, CURRENT_SCORE_WIDGET_ID), buffer);
    }
    if (completedLines != oldCompletedLines)
    {
        char buffer[7];
        sprintf(buffer, "%06d", completedLines);
        win_widget_name_set(win_widget(window, COMPLETE_LINES_WIDGET_ID), buffer);
    }
    if (playedBlocks != oldPlayedBlocks)
    {
        char buffer[7];
        sprintf(buffer, "%06d", playedBlocks);
        win_widget_name_set(win_widget(window, PLAYED_BLOCKS_WIDGET_ID), buffer);
    }

    oldCurrentScore = currentScore;
    oldCompletedLines = completedLines;
    oldPlayedBlocks = playedBlocks;

    return 0;
}

int main(void)
{
    rect_t rect = RECT_INIT_DIM(500, 200, WINDOW_WIDTH, WINDOW_HEIGHT);
    win_expand_to_window(&rect, WIN_DECO);
    win_t* window = win_new("Tetris", &rect, DWM_WINDOW, WIN_DECO, procedure);
    if (window == NULL)
    {
        exit(errno);
    }

    msg_t msg = {0};
    while (msg.type != LMSG_QUIT)
    {
        win_receive(window, &msg, NEVER);
        win_dispatch(window, &msg);
    }

    win_free(window);
    return EXIT_SUCCESS;
}

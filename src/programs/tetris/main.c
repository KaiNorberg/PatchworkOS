#include <errno.h>
#include <libpatchwork/patchwork.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/proc.h>

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
#define SIDE_PANEL_LABEL_PADDING 40

#define WINDOW_WIDTH ((FIELD_WIDTH) * BLOCK_SIZE + FIELD_PADDING * 2 + SIDE_PANEL_WIDTH)
#define WINDOW_HEIGHT ((FIELD_HEIGHT) * BLOCK_SIZE + FIELD_PADDING * 2)

#define CURRENT_SCORE_LABEL_ID 0
#define COMPLETE_LINES_LABEL_ID 1
#define PLAYED_BLOCKS_LABEL_ID 2

#define TICK_SPEED (CLOCKS_PER_SEC)
#define DROPPING_TICK_SPEED (CLOCKS_PER_SEC / 12)
#define CLEARING_LINES_TICK_SPEED (CLOCKS_PER_SEC / 15)
#define START_SCREEN_TICK_SPEED ((CLOCKS_PER_SEC / 4) * 3)

#define PIECE_AMOUNT 7
#define PIECE_WIDTH 4
#define PIECE_HEIGHT 4

static element_t* currentScoreLabel;
static element_t* completeLinesLabel;
static element_t* playedBlocksLabel;

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

static bool isClearingLines;
static bool isStarted;
static bool isGameover;

static font_t* largeFont;
static font_t* massiveFont;

static struct
{
    piece_t piece;
    int64_t x;
    int64_t y;
    bool isDropping;
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
static void current_piece_clear(element_t* elem, drawable_t* draw);
static void current_piece_draw(element_t* elem, drawable_t* draw);

static void block_draw(element_t* elem, drawable_t* draw, block_t block, int64_t x, int64_t y)
{
    if (x < 0 || y < 0 || x >= FIELD_WIDTH || y >= FIELD_HEIGHT)
    {
        return;
    }

    rect_t rect = RECT_INIT_DIM(FIELD_LEFT + x * BLOCK_SIZE, FIELD_TOP + y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE);

    const theme_t* theme = element_get_theme(elem);

    draw_frame(draw, &rect, theme->frameSize, highlightColors[block], shadowColors[block]);
    RECT_SHRINK(&rect, theme->frameSize);
    draw_rect(draw, &rect, normalColors[block]);
    RECT_SHRINK(&rect, 5);
    draw_frame(draw, &rect, theme->frameSize, shadowColors[block], highlightColors[block]);
}

static void side_panel_draw(element_t* elem, drawable_t* draw)
{
    rect_t rect = RECT_INIT(SIDE_PANEL_LEFT, SIDE_PANEL_TOP, SIDE_PANEL_RIGHT, SIDE_PANEL_BOTTOM);

    const theme_t* theme = element_get_theme(elem);

    draw_ridge(draw, &rect, theme->frameSize, theme->deco.highlight, theme->deco.shadow);

    rect_t textRect = rect;
    textRect.bottom = textRect.top + SIDE_PANEL_TEXT_HEIGHT;
    draw_text(draw, &textRect, largeFont, ALIGN_CENTER, ALIGN_CENTER, theme->view.foregroundNormal, "Score");

    textRect.top = textRect.bottom + SIDE_PANEL_LABEL_HEIGHT;
    textRect.bottom = textRect.top + SIDE_PANEL_TEXT_HEIGHT;
    draw_text(draw, &textRect, largeFont, ALIGN_CENTER, ALIGN_CENTER, theme->view.foregroundNormal, "Lines");

    textRect.top = textRect.bottom + SIDE_PANEL_LABEL_HEIGHT;
    textRect.bottom = textRect.top + SIDE_PANEL_TEXT_HEIGHT;
    draw_text(draw, &textRect, largeFont, ALIGN_CENTER, ALIGN_CENTER, theme->view.foregroundNormal, "Pieces");

    uint64_t fontHeight = font_height(largeFont);

    textRect.top = rect.bottom - fontHeight * 7;
    textRect.bottom = rect.bottom;
    draw_text(draw, &textRect, largeFont, ALIGN_CENTER, ALIGN_CENTER, theme->view.foregroundNormal, "  ASD - Move");
    textRect.top += fontHeight;
    textRect.bottom += fontHeight;
    draw_text(draw, &textRect, largeFont, ALIGN_CENTER, ALIGN_CENTER, theme->view.foregroundNormal, "SPACE - Drop");
    textRect.top += fontHeight;
    textRect.bottom += fontHeight;
    draw_text(draw, &textRect, largeFont, ALIGN_CENTER, ALIGN_CENTER, theme->view.foregroundNormal, "    R - Spin");
}

static point_t piece_block_pos_in_field(int64_t pieceX, int64_t pieceY, int64_t blockX, int64_t blockY)
{
    return (point_t){.x = pieceX + blockX - PIECE_WIDTH / 2, .y = pieceY + blockY - PIECE_HEIGHT / 2};
}

static bool piece_is_out_of_bounds(const piece_t* piece, int64_t pieceX, int64_t pieceY)
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
            if (point.x < 0 || point.x >= FIELD_WIDTH ||
                point.y >= FIELD_HEIGHT) // point.y < 0 check is left out on purpose
            {
                return true;
            }
        }
    }

    return false;
}

static void piece_clear(element_t* elem, drawable_t* draw, const piece_t* piece, uint64_t pieceX, uint64_t pieceY)
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
            block_draw(elem, draw, BLOCK_NONE, point.x, point.y);
        }
    }
}

static void piece_outline_draw(element_t* elem, drawable_t* draw, const piece_t* piece, uint64_t pieceX,
    uint64_t pieceY)
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
            block_draw(elem, draw, BLOCK_OUTLINE, point.x, point.y);
        }
    }
}

static void piece_draw(element_t* elem, drawable_t* draw, const piece_t* piece, uint64_t pieceX, uint64_t pieceY)
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
            block_draw(elem, draw, (*piece)[blockY][blockX], point.x, point.y);
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

static void field_edge_draw(element_t* elem, drawable_t* draw)
{
    const theme_t* theme = element_get_theme(elem);

    rect_t fieldRect = RECT_INIT(FIELD_LEFT, FIELD_TOP, FIELD_RIGHT, FIELD_BOTTOM);
    RECT_EXPAND(&fieldRect, FIELD_PADDING);
    draw_bezel(draw, &fieldRect, FIELD_PADDING - theme->frameSize, theme->deco.backgroundNormal);
    RECT_SHRINK(&fieldRect, FIELD_PADDING - theme->frameSize);
    draw_frame(draw, &fieldRect, theme->frameSize, theme->deco.shadow, theme->deco.highlight);
}

static void field_draw(element_t* elem, drawable_t* draw)
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

            block_draw(elem, draw, field[y][x], x, y);
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

static void field_clear_lines(element_t* elem, drawable_t* draw)
{
    current_piece_clear(elem, draw);
    bool isDone = true;
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

        isDone = false;
    }

    if (!isDone)
    {
        field_draw(elem, draw);
    }
    else
    {
        isClearingLines = false;
    }

    current_piece_draw(elem, draw);
}

static void field_check_for_lines(element_t* elem, drawable_t* draw)
{
    uint64_t foundLines = 0;
    for (uint64_t y = 0; y < FIELD_HEIGHT; y++)
    {
        bool isLineComplete = true;
        for (uint64_t x = 0; x < FIELD_WIDTH; x++)
        {
            if (field[y][x] == BLOCK_NONE)
            {
                isLineComplete = false;
                break;
            }
        }

        if (isLineComplete)
        {
            for (uint64_t x = 0; x < FIELD_WIDTH; x++)
            {
                field[y][x] = BLOCK_CLEARING;
            }
            isClearingLines = true;
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

    field_draw(elem, draw);
}

static void pause()
{
    isClearingLines = false;

    for (int64_t y = 0; y < FIELD_HEIGHT; y++)
    {
        for (int64_t x = 0; x < FIELD_WIDTH; x++)
        {
            field[y][x] = BLOCK_NONE;
            oldField[y][x] = BLOCK_INVAL;
        }
    }

    isStarted = false;
    isGameover = false;
}

static void start()
{
    currentScore = 0;
    completedLines = 0;
    playedBlocks = 0;

    isClearingLines = false;

    for (int64_t y = 0; y < FIELD_HEIGHT; y++)
    {
        for (int64_t x = 0; x < FIELD_WIDTH; x++)
        {
            field[y][x] = BLOCK_NONE;
            oldField[y][x] = BLOCK_INVAL;
        }
    }

    current_piece_choose_new();
    currentPiece.isDropping = false;

    isStarted = true;
    isGameover = false;
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
        isGameover = true;
    }
}

static void current_piece_clear(element_t* elem, drawable_t* draw)
{
    int64_t outlineY = currentPiece.y;
    while (!piece_is_out_of_bounds(&currentPiece.piece, currentPiece.x, outlineY) &&
        !field_collides(&currentPiece.piece, currentPiece.x, outlineY))
    {
        outlineY++;
    }
    outlineY--;

    piece_clear(elem, draw, &currentPiece.piece, currentPiece.x, outlineY);
    piece_clear(elem, draw, &currentPiece.piece, currentPiece.x, currentPiece.y);
}

static void current_piece_draw(element_t* elem, drawable_t* draw)
{
    int64_t outlineY = currentPiece.y;
    while (!piece_is_out_of_bounds(&currentPiece.piece, currentPiece.x, outlineY) &&
        !field_collides(&currentPiece.piece, currentPiece.x, outlineY))
    {
        outlineY++;
    }
    outlineY--;

    piece_outline_draw(elem, draw, &currentPiece.piece, currentPiece.x, outlineY);
    piece_draw(elem, draw, &currentPiece.piece, currentPiece.x, currentPiece.y);
}

static void current_piece_update(element_t* elem, drawable_t* draw)
{
    if (piece_is_out_of_bounds(&currentPiece.piece, currentPiece.x, currentPiece.y + 1) ||
        field_collides(&currentPiece.piece, currentPiece.x, currentPiece.y + 1))
    {
        field_add_piece(&currentPiece.piece, currentPiece.x, currentPiece.y);
        current_piece_choose_new();
        current_piece_draw(elem, draw);
        field_check_for_lines(elem, draw);
    }
    else
    {
        current_piece_clear(elem, draw);
        currentPiece.y++;
        current_piece_draw(elem, draw);
    }
}

static void current_piece_move(element_t* elem, drawable_t* draw, keycode_t code)
{
    uint64_t newX = currentPiece.x + (code == KBD_D) - (code == KBD_A);

    if (piece_is_out_of_bounds(&currentPiece.piece, newX, currentPiece.y) ||
        field_collides(&currentPiece.piece, newX, currentPiece.y))
    {
        return;
    }

    current_piece_clear(elem, draw);
    currentPiece.x = newX;
    current_piece_draw(elem, draw);
}

static void current_piece_drop(element_t* elem, drawable_t* draw)
{
    current_piece_clear(elem, draw);

    while (!piece_is_out_of_bounds(&currentPiece.piece, currentPiece.x, currentPiece.y) &&
        !field_collides(&currentPiece.piece, currentPiece.x, currentPiece.y))
    {
        currentPiece.y++;
    }
    currentPiece.y--;

    current_piece_draw(elem, draw);
}

static void current_piece_rotate(element_t* elem, drawable_t* draw)
{
    piece_t rotatedPiece;
    memcpy(&rotatedPiece, &currentPiece.piece, sizeof(piece_t));
    piece_rotate(&rotatedPiece);

    if (piece_is_out_of_bounds(&rotatedPiece, currentPiece.x, currentPiece.y) ||
        field_collides(&rotatedPiece, currentPiece.x, currentPiece.y))
    {
        return;
    }

    current_piece_clear(elem, draw);
    memcpy(&currentPiece.piece, &rotatedPiece, sizeof(piece_t));
    current_piece_draw(elem, draw);
}

static void start_tetris_draw(drawable_t* draw)
{
    uint64_t totalWidth = font_width(massiveFont, "TETRIS", 6);

    rect_t rect = RECT_INIT((FIELD_RIGHT + FIELD_LEFT) / 2 - totalWidth / 2 - 10, FIELD_TOP,
        (FIELD_RIGHT + FIELD_LEFT) / 2 - totalWidth / 3 + 10, FIELD_TOP + (FIELD_BOTTOM - FIELD_TOP) / 2);

    draw_text(draw, &rect, massiveFont, ALIGN_CENTER, ALIGN_CENTER, normalColors[BLOCK_RED], "T");
    uint64_t width = font_width(massiveFont, "T", 1);
    rect.left += width;
    rect.right += width;
    draw_text(draw, &rect, massiveFont, ALIGN_CENTER, ALIGN_CENTER, normalColors[BLOCK_ORANGE], "E");
    width = font_width(massiveFont, "E", 1);
    rect.left += width;
    rect.right += width;
    draw_text(draw, &rect, massiveFont, ALIGN_CENTER, ALIGN_CENTER, normalColors[BLOCK_YELLOW], "T");
    width = font_width(massiveFont, "T", 1);
    rect.left += width;
    rect.right += width;
    draw_text(draw, &rect, massiveFont, ALIGN_CENTER, ALIGN_CENTER, normalColors[BLOCK_GREEN], "R");
    width = font_width(massiveFont, "R", 1);
    rect.left += width - 8;
    rect.right += width - 8;
    draw_text(draw, &rect, massiveFont, ALIGN_CENTER, ALIGN_CENTER, normalColors[BLOCK_CYAN], "I");
    width = font_width(massiveFont, "I", 1);
    rect.left += width + 8;
    rect.right += width + 8;
    draw_text(draw, &rect, massiveFont, ALIGN_CENTER, ALIGN_CENTER, normalColors[BLOCK_BLUE], "S");
}

static void start_press_space_draw(element_t* elem, drawable_t* draw)
{
    static bool blink = false;

    rect_t rect = RECT_INIT(FIELD_LEFT, (FIELD_TOP + FIELD_BOTTOM) / 2, FIELD_RIGHT, FIELD_BOTTOM);
    draw_rect(draw, &rect, normalColors[BLOCK_NONE]);
    if (blink)
    {
        const theme_t* theme = element_get_theme(elem);
        draw_text(draw, &rect, largeFont, ALIGN_CENTER, ALIGN_CENTER, theme->deco.foregroundNormal, "PRESS SPACE");
    }
    blink = !blink;
}

static uint64_t procedure(window_t* win, element_t* elem, const event_t* event)
{
    const theme_t* theme = element_get_theme(elem);

    switch (event->type)
    {
    case LEVENT_INIT:
    {
        srand(uptime());

        currentScore = 0;
        completedLines = 0;
        playedBlocks = 0;

        rect_t labelRect = RECT_INIT(SIDE_PANEL_LEFT + SIDE_PANEL_LABEL_PADDING,
            SIDE_PANEL_TOP + SIDE_PANEL_TEXT_HEIGHT, SIDE_PANEL_RIGHT - SIDE_PANEL_LABEL_PADDING,
            SIDE_PANEL_TOP + SIDE_PANEL_TEXT_HEIGHT + SIDE_PANEL_LABEL_HEIGHT);
        currentScoreLabel = label_new(elem, CURRENT_SCORE_LABEL_ID, &labelRect, "000000", ELEMENT_NONE);
        element_get_text_props(currentScoreLabel)->font = largeFont;
        element_get_theme(currentScoreLabel)->view.backgroundNormal = theme->view.foregroundNormal;
        element_get_theme(currentScoreLabel)->view.foregroundNormal = theme->view.backgroundNormal;

        labelRect.top = labelRect.bottom + SIDE_PANEL_LABEL_HEIGHT;
        labelRect.bottom = labelRect.top + SIDE_PANEL_TEXT_HEIGHT;
        completeLinesLabel = label_new(elem, COMPLETE_LINES_LABEL_ID, &labelRect, "000000", ELEMENT_NONE);
        element_get_text_props(completeLinesLabel)->font = largeFont;
        element_get_theme(completeLinesLabel)->view.backgroundNormal = theme->view.foregroundNormal;
        element_get_theme(completeLinesLabel)->view.foregroundNormal = theme->view.backgroundNormal;

        labelRect.top = labelRect.bottom + SIDE_PANEL_LABEL_HEIGHT;
        labelRect.bottom = labelRect.top + SIDE_PANEL_TEXT_HEIGHT;
        playedBlocksLabel = label_new(elem, PLAYED_BLOCKS_LABEL_ID, &labelRect, "000000", ELEMENT_NONE);
        element_get_text_props(playedBlocksLabel)->font = largeFont;
        element_get_theme(playedBlocksLabel)->view.backgroundNormal = theme->view.foregroundNormal;
        element_get_theme(playedBlocksLabel)->view.foregroundNormal = theme->view.backgroundNormal;

        pause();
    }
    break;
    case LEVENT_QUIT:
    {
        display_disconnect(window_get_display(win));
    }
    break;
    case LEVENT_REDRAW:
    {
        drawable_t draw;
        element_draw_begin(elem, &draw);

        field_edge_draw(elem, &draw);
        field_draw(elem, &draw);
        side_panel_draw(elem, &draw);

        element_draw_end(elem, &draw);

        window_set_timer(win, TIMER_NONE, 0);
    }
    break;
    case EVENT_TIMER:
    {
        drawable_t draw;
        element_draw_begin(elem, &draw);

        if (!isStarted)
        {
            start_tetris_draw(&draw);
            start_press_space_draw(elem, &draw);
            window_set_timer(win, TIMER_NONE, START_SCREEN_TICK_SPEED);

            element_draw_end(elem, &draw);
            break;
        }
        else if (isClearingLines)
        {
            field_clear_lines(elem, &draw);
            window_set_timer(win, TIMER_NONE, CLEARING_LINES_TICK_SPEED);

            element_draw_end(elem, &draw);
            break;
        }
        else if (currentPiece.isDropping)
        {
            window_set_timer(win, TIMER_NONE, DROPPING_TICK_SPEED);
        }
        else
        {
            window_set_timer(win, TIMER_NONE, TICK_SPEED);
        }

        current_piece_update(elem, &draw);

        if (isClearingLines || isGameover)
        {
            isGameover = false;
            window_set_timer(win, TIMER_NONE, 0);
        }

        element_draw_end(elem, &draw);
    }
    break;
    case EVENT_KBD:
    {
        if (!isStarted)
        {
            if (event->kbd.type == KBD_PRESS && event->kbd.code == KBD_SPACE)
            {
                start();
                element_redraw(elem, false);
            }

            break;
        }
        else if (isClearingLines)
        {
            currentPiece.isDropping = false;
            break;
        }

        drawable_t draw;
        element_draw_begin(elem, &draw);

        if (event->kbd.type == KBD_PRESS && (event->kbd.code == KBD_A || event->kbd.code == KBD_D))
        {
            current_piece_move(elem, &draw, event->kbd.code);
        }
        else if (event->kbd.type == KBD_PRESS && event->kbd.code == KBD_R)
        {
            current_piece_rotate(elem, &draw);
        }
        else if (event->kbd.type == KBD_PRESS && event->kbd.code == KBD_S)
        {
            currentPiece.isDropping = true;
            window_set_timer(win, TIMER_NONE, 0);
        }
        else if (event->kbd.type == KBD_PRESS && event->kbd.code == KBD_SPACE)
        {
            current_piece_drop(elem, &draw);
            window_set_timer(win, TIMER_NONE, 0);
        }
        else if (event->kbd.type == KBD_RELEASE && event->kbd.code == KBD_S)
        {
            currentPiece.isDropping = false;
            window_set_timer(win, TIMER_NONE, TICK_SPEED);
        }

        element_draw_end(elem, &draw);
    }
    break;
    }

    if (currentScore != oldCurrentScore)
    {
        char buffer[7];
        sprintf(buffer, "%06d", currentScore);
        element_set_text(currentScoreLabel, buffer);
        element_redraw(currentScoreLabel, false);
    }
    if (completedLines != oldCompletedLines)
    {
        char buffer[7];
        sprintf(buffer, "%06d", completedLines);
        element_set_text(completeLinesLabel, buffer);
        element_redraw(completeLinesLabel, false);
    }
    if (playedBlocks != oldPlayedBlocks)
    {
        char buffer[7];
        sprintf(buffer, "%06d", playedBlocks);
        element_set_text(playedBlocksLabel, buffer);
        element_redraw(playedBlocksLabel, false);
    }

    oldCurrentScore = currentScore;
    oldCompletedLines = completedLines;
    oldPlayedBlocks = playedBlocks;

    return 0;
}

int main(void)
{
    display_t* disp = display_new();
    if (disp == NULL)
    {
        return EXIT_FAILURE;
    }

    largeFont = font_new(disp, "default", "regular", 32);
    if (largeFont == NULL)
    {
        display_free(disp);
        return EXIT_FAILURE;
    }
    massiveFont = font_new(disp, "default", "regular", 64);
    if (massiveFont == NULL)
    {
        font_free(largeFont);
        display_free(disp);
        return EXIT_FAILURE;
    }

    rect_t rect = RECT_INIT_DIM(500, 200, WINDOW_WIDTH, WINDOW_HEIGHT);
    window_t* win = window_new(disp, "Tetris", &rect, SURFACE_WINDOW, WINDOW_DECO, procedure, NULL);
    if (win == NULL)
    {
        return EXIT_FAILURE;
    }

    window_set_visible(win, true);

    event_t event = {0};
    while (display_next_event(disp, &event, CLOCKS_NEVER) != ERR)
    {
        display_dispatch(disp, &event);
    }

    window_free(win);
    font_free(massiveFont);
    font_free(largeFont);
    display_free(disp);
    return EXIT_SUCCESS;
}

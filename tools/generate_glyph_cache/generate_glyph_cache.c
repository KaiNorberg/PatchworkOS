#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FONT_HEIGHT 16
#define FONT_WIDTH 8

#define SCREEN_CHAR_AMOUNT (256)

#define SCREEN_COLOR_TEXT 0xFFA3A4A3

typedef struct
{
    uint32_t pixels[FONT_HEIGHT * FONT_WIDTH];
} screen_glyph_t;

typedef struct
{
    screen_glyph_t glyphs[SCREEN_CHAR_AMOUNT];
} screen_glyph_cache_t;

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <psf_font_file>\n", argv[0]);
        return 1;
    }

    FILE* file = fopen(argv[1], "rb");
    if (!file)
    {
        perror("Error opening font file");
        return 1;
    }

    if (fseek(file, 4, SEEK_SET) != 0)
    {
        perror("Error seeking past font file header");
        fclose(file);
        return 1;
    }

    printf("#include <stdint.h>\n\n");
    printf("#define FONT_HEIGHT %d\n", FONT_HEIGHT);
    printf("#define FONT_WIDTH %d\n", FONT_WIDTH);
    printf("#define SCREEN_CHAR_AMOUNT %d\n\n", SCREEN_CHAR_AMOUNT);

    printf("typedef struct\n");
    printf("{\n");
    printf("    uint32_t pixels[FONT_HEIGHT * FONT_WIDTH];\n");
    printf("} screen_glyph_t;\n\n");

    printf("typedef struct\n");
    printf("{\n");
    printf("    screen_glyph_t glyphs[SCREEN_CHAR_AMOUNT];\n");
    printf("} screen_glyph_cache_t;\n\n");

    printf("const screen_glyph_cache_t screenGlyphCache = {\n");
    printf("    .glyphs = {\n");

    uint8_t glyphData[FONT_HEIGHT];

    for (int i = 0; i < SCREEN_CHAR_AMOUNT; ++i)
    {
        if (fread(glyphData, 1, FONT_HEIGHT, file) != FONT_HEIGHT)
        {
            if (feof(file))
            {
                fprintf(stderr, "Warning: Reached end of file before processing all %d glyphs. Processed %d glyphs.\n",
                    SCREEN_CHAR_AMOUNT, i);
            }
            else
            {
                perror("Error reading glyph data from font file");
            }

            for (int j = i; j < SCREEN_CHAR_AMOUNT; ++j)
            {
                printf("{\n");
                printf(".pixels = {\n");
                for (int k = 0; k < FONT_HEIGHT * FONT_WIDTH; ++k)
                {
                    printf("                0x00000000");
                    if (k < (FONT_HEIGHT * FONT_WIDTH) - 1)
                    {
                        printf(",");
                        if ((k + 1) % FONT_WIDTH == 0)
                        {
                            printf("\n");
                        }
                        else
                        {
                            printf(" ");
                        }
                    }
                    else
                    {
                        printf("\n");
                    }
                }
                printf("}\n");
                printf("}%s\n", (j < SCREEN_CHAR_AMOUNT - 1) ? "," : "");
            }
            break;
        }

        printf("{\n");
        printf(".pixels = {\n");

        for (int y = 0; y < FONT_HEIGHT; ++y)
        {
            uint8_t rowByte = glyphData[y];

            for (int x = 0; x < FONT_WIDTH; ++x)
            {
                uint32_t pixel = (rowByte & (0x80 >> x)) ? SCREEN_COLOR_TEXT : 0x00000000;
                printf("0x%08X", pixel);

                if (y == FONT_HEIGHT - 1 && x == FONT_WIDTH - 1)
                {
                    printf("\n");
                }
                else
                {
                    printf(",");
                    if (x == FONT_WIDTH - 1)
                    {
                        printf("\n");
                    }
                    else
                    {
                        printf(" ");
                    }
                }
            }
        }
        printf("}\n");
        printf("}%s\n", (i < SCREEN_CHAR_AMOUNT - 1) ? "," : "");
    }

    printf("}\n");
    printf("};\n");

    fclose(file);

    return 0;
}
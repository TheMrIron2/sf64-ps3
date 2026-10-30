#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <stdlib.h>

// Generate the fixed ASCII atlas with a host FreeType library
int main(int argc, char** argv) {
    FT_Library library;
    FT_Face face;
    unsigned char atlas[256 * 128] = {0};
    int metrics[95][5];
    int ch, x, y, i;
    FILE* out;
    if (argc != 3) return 1;
    if (FT_Init_FreeType(&library) || FT_New_Face(library, argv[1], 0, &face) ||
        FT_Set_Pixel_Sizes(face, 0, 16)) return 1;
    for (ch = 32; ch < 127; ch++) {
        FT_GlyphSlot g;
        if (FT_Load_Char(face, ch, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL)) return 1;
        g = face->glyph;
        if (g->bitmap.width > 16 || g->bitmap.rows > 16 || g->bitmap.pixel_mode != FT_PIXEL_MODE_GRAY) return 1;
        metrics[ch-32][0] = g->bitmap.width;
        metrics[ch-32][1] = g->bitmap.rows;
        metrics[ch-32][2] = g->bitmap_left;
        metrics[ch-32][3] = 15 - g->bitmap_top;
        metrics[ch-32][4] = (g->advance.x + 32) / 64;
        for (y = 0; y < (int)g->bitmap.rows; y++) {
            for (x = 0; x < (int)g->bitmap.width; x++) {
                atlas[((ch-32)/16*16+y)*256+(ch-32)%16*16+x] =
                    (g->bitmap.buffer[y*g->bitmap.pitch+x] + 8) / 17;
            }
        }
    }
    out = fopen(argv[2], "w");
    if (!out) return 1;
    fprintf(out, "// Generated from DejaVu Sans at 16 pixels with bake_menu_font.c\n");
    fprintf(out, "// Font copyright and permission are in LICENSE.DejaVu\n");
    fprintf(out, "typedef struct { u8 width, height; s8 left, top; u8 advance; } PspMenuGlyph;\n");
    fprintf(out, "static const PspMenuGlyph sMenuGlyphs[95] = {\n");
    for (i = 0; i < 95; i++) fprintf(out, "    {%d,%d,%d,%d,%d},\n", metrics[i][0],metrics[i][1],metrics[i][2],metrics[i][3],metrics[i][4]);
    fprintf(out, "};\n\nstatic const u8 sMenuFontRle[] = {\n    ");
    for (i = 0, ch = 0; i < (int)sizeof(atlas); ch++) {
        int count = 1;
        while (count < 16 && i + count < (int)sizeof(atlas) && atlas[i+count] == atlas[i]) count++;
        fprintf(out, "0x%02x,", ((count-1)<<4)|atlas[i]);
        if ((ch+1)%16 == 0) fprintf(out, "\n    ");
        i += count;
    }
    fprintf(out, "\n};\n");
    if (fclose(out)) return 1;
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return 0;
}

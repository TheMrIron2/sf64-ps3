#include <stdint.h>
#include "src/psp/overlay.h"
#include "src/psp/gfx/gfx_psp_backend.h"
#include "src/psp/gfx/gfx_psp_device.h"
#include "src/psp/fonts/menu_font.h"
#if PSP_GFX_BACKEND_GU
#include "src/psp/overlay_gu.h"
static int sGuDrawing;
#endif

#if PSP_GFX_BACKEND_PSPGL
#include <GLES/gl.h>
#endif

#if PSP_GFX_BACKEND_GU || !PSP_GFX_BACKEND_PSPGL
static void* sPixels;
static int sStride;
static int sFormat;
#endif

// Five columns per glyph with the least significant bit at the top
static const u8 sFont[][5] = {
    {0,0,0,0,0}, {0,0,95,0,0}, {0,7,0,7,0}, {20,127,20,127,20},
    {36,42,127,42,18}, {35,19,8,100,98}, {54,73,85,34,80}, {0,5,3,0,0},
    {0,28,34,65,0}, {0,65,34,28,0}, {20,8,62,8,20}, {8,8,62,8,8},
    {0,80,48,0,0}, {8,8,8,8,8}, {0,96,96,0,0}, {32,16,8,4,2},
    {62,81,73,69,62}, {0,66,127,64,0}, {66,97,81,73,70}, {33,65,69,75,49},
    {24,20,18,127,16}, {39,69,69,69,57}, {60,74,73,73,48}, {1,113,9,5,3},
    {54,73,73,73,54}, {6,73,73,41,30}, {0,54,54,0,0}, {0,86,54,0,0},
    {8,20,34,65,0}, {20,20,20,20,20}, {0,65,34,20,8}, {2,1,81,9,6},
    {50,73,121,65,62}, {126,17,17,17,126}, {127,73,73,73,54}, {62,65,65,65,34},
    {127,65,65,34,28}, {127,73,73,73,65}, {127,9,9,9,1}, {62,65,73,73,122},
    {127,8,8,8,127}, {0,65,127,65,0}, {32,64,65,63,1}, {127,8,20,34,65},
    {127,64,64,64,64}, {127,2,12,2,127}, {127,4,8,16,127}, {62,65,65,65,62},
    {127,9,9,9,6}, {62,65,81,33,94}, {127,9,25,41,70}, {70,73,73,73,49},
    {1,1,127,1,1}, {63,64,64,64,63}, {31,32,64,32,31}, {63,64,56,64,63},
    {99,20,8,20,99}, {7,8,112,8,7}, {97,81,73,69,67}, {0,127,65,65,0},
    {2,4,8,16,32}, {0,65,65,127,0}, {4,2,1,2,4}, {64,64,64,64,64}
};

#if PSP_GFX_BACKEND_PSPGL
typedef struct { float u, v; u32 color; float x, y, z; } OverlayVertex;
static OverlayVertex sVertices[768];
static int sCount;
static GLuint sFontTexture;
static int sTextured;

static void overlay_flush(void) {
    if (!sCount) return;
    glTexCoordPointer(2, GL_FLOAT, sizeof(OverlayVertex), &sVertices[0].u);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(OverlayVertex), &sVertices[0].color);
    glVertexPointer(3, GL_FLOAT, sizeof(OverlayVertex), &sVertices[0].x);
    glDrawArrays(GL_TRIANGLES, 0, sCount);
    sCount = 0;
}

static void overlay_texture(int enabled) {
    if (sTextured == enabled) return;
    overlay_flush();
    if (enabled) glEnable(GL_TEXTURE_2D);
    else glDisable(GL_TEXTURE_2D);
    sTextured = enabled;
}

static void overlay_font(void) {
    static u16 pixels[128 * 32];
    int ch, x, y;
    if (!sFontTexture) {
        for (ch = 0; ch < 64; ch++) {
            for (y = 0; y < 8; y++) {
                for (x = 0; x < 8; x++) {
                    int set = x < 5 && y < 7 && (sFont[ch][x] & (1 << y));
                    pixels[(ch / 16 * 8 + y) * 128 + ch % 16 * 8 + x] = set ? 0xFFFF : 0xFFF0;
                }
            }
        }
        glGenTextures(1, &sFontTexture);
        glBindTexture(GL_TEXTURE_2D, sFontTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 32, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, pixels);
    } else {
        glBindTexture(GL_TEXTURE_2D, sFontTexture);
    }
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}
#endif

int PspOverlay_Begin(void) {
#if PSP_GFX_BACKEND_PSPGL
    if (!PspGfxBackend_IsGu()) {
    sCount = 0;
    sTextured = 0;
    overlay_font();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_FOG);
    glDisable(GL_LIGHTING);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_FALSE);
    glViewport(0, 0, 480, 272);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0, 480, 272, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    return 1;
    }
#endif
#if PSP_GFX_BACKEND_GU || !PSP_GFX_BACKEND_PSPGL
#if PSP_GFX_BACKEND_GU
    sGuDrawing = 0;
#endif
    sPixels = PspGfxDevice_GetOverlayFrameBuffer(&sStride, &sFormat);
    if (!sPixels || sStride != 512 || sFormat < 0 || sFormat > 3) return 0;
    sPixels = (void*)((uintptr_t)sPixels | 0x40000000U);
    return 1;
#endif
    return 0;
}

#if PSP_GFX_BACKEND_GU || !PSP_GFX_BACKEND_PSPGL
static u32 overlay_unpack(u32 p) {
    if (sFormat == 3) return p;
    if (sFormat == 2) return ((p & 15) * 17) | (((p >> 4) & 15) * 17 << 8) |
                              (((p >> 8) & 15) * 17 << 16) | 0xFF000000U;
    if (sFormat == 1) return ((p & 31) * 255 / 31) | (((p >> 5) & 31) * 255 / 31 << 8) |
                              (((p >> 10) & 31) * 255 / 31 << 16) | 0xFF000000U;
    return ((p & 31) * 255 / 31) | (((p >> 5) & 63) * 255 / 63 << 8) |
           (((p >> 11) & 31) * 255 / 31 << 16) | 0xFF000000U;
}

static u32 overlay_pack(u32 p) {
    if (sFormat == 3) return p;
    if (sFormat == 2) return ((p >> 4) & 15) | ((p >> 8) & 240) | ((p >> 12) & 3840) | 0xF000;
    if (sFormat == 1) return ((p >> 3) & 31) | ((p >> 6) & 992) | ((p >> 9) & 31744) | 0x8000;
    return ((p >> 3) & 31) | ((p >> 5) & 2016) | ((p >> 8) & 63488);
}
#endif

void PspOverlay_Rect(int x, int y, int width, int height, u32 color) {
    int right = x + width;
    int bottom = y + height;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (right > 480) right = 480;
    if (bottom > 272) bottom = 272;
    if (right <= x || bottom <= y) return;
#if PSP_GFX_BACKEND_GU
    if (sGuDrawing) {
        PspOverlayGu_Rect(x, y, right - x, bottom - y, color);
        return;
    }
#endif
#if PSP_GFX_BACKEND_PSPGL
    if (!PspGfxBackend_IsGu()) {
    {
        OverlayVertex v[6] = {
            {0,0,color,x,y,0}, {0,0,color,right,y,0}, {0,0,color,x,bottom,0},
            {0,0,color,x,bottom,0}, {0,0,color,right,y,0}, {0,0,color,right,bottom,0}
        };
        int i;
        overlay_texture(0);
        if (sCount + 6 > 768) overlay_flush();
        for (i = 0; i < 6; i++) sVertices[sCount++] = v[i];
    }
        return;
    }
#endif
#if PSP_GFX_BACKEND_GU || !PSP_GFX_BACKEND_PSPGL
    {
        int ix, iy;
        unsigned alpha = color >> 24;
        for (iy = y; iy < bottom; iy++) {
            for (ix = x; ix < right; ix++) {
                int offset = iy * sStride + ix;
                u32 result = color;
                if (alpha != 255) {
                    u32 old = overlay_unpack(sFormat == 3 ? ((u32*)sPixels)[offset] : ((u16*)sPixels)[offset]);
                    u32 r = ((color & 255) * alpha + (old & 255) * (255 - alpha) + 127) / 255;
                    u32 g = (((color >> 8) & 255) * alpha + ((old >> 8) & 255) * (255 - alpha) + 127) / 255;
                    u32 b = (((color >> 16) & 255) * alpha + ((old >> 16) & 255) * (255 - alpha) + 127) / 255;
                    result = r | (g << 8) | (b << 16) | 0xFF000000U;
                }
                if (sFormat == 3) ((u32*)sPixels)[offset] = result;
                else ((u16*)sPixels)[offset] = overlay_pack(result);
            }
        }
    }
#endif
}

void PspOverlay_Text(int x, int y, u32 color, const char* text) {
#if PSP_GFX_BACKEND_GU
    if (sGuDrawing) {
        while (*text && x + 5 <= 480) {
            unsigned ch = (u8)*text++;
            if (ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
            if (ch < 32 || ch > 95) ch = '?';
            PspOverlayGu_Glyph(x, y, (ch - 32) % 16 * 8, (ch - 32) / 16 * 8, 5, 7, color, 0);
            x += 6;
        }
        return;
    }
#endif
#if PSP_GFX_BACKEND_PSPGL
    if (!PspGfxBackend_IsGu()) {
        while (*text && x + 5 <= 480) {
            unsigned ch = (u8)*text++;
            float u, v, u1, v1;
            OverlayVertex vertices[6];
            int i;
            if (ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
            if (ch < 32 || ch > 95) ch = '?';
            u = ((ch - 32) % 16 * 8) / 128.0f;
            v = ((ch - 32) / 16 * 8) / 32.0f;
            u1 = u + 5 / 128.0f;
            v1 = v + 7 / 32.0f;
            vertices[0] = (OverlayVertex){u,v,color,x,y,0};
            vertices[1] = (OverlayVertex){u1,v,color,x+5,y,0};
            vertices[2] = (OverlayVertex){u,v1,color,x,y+7,0};
            vertices[3] = vertices[2];
            vertices[4] = vertices[1];
            vertices[5] = (OverlayVertex){u1,v1,color,x+5,y+7,0};
            overlay_texture(1);
            if (sCount + 6 > 768) overlay_flush();
            for (i = 0; i < 6; i++) sVertices[sCount++] = vertices[i];
            x += 6;
        }
        return;
    }
#endif
#if PSP_GFX_BACKEND_GU || !PSP_GFX_BACKEND_PSPGL
    while (*text && x + 5 <= 480) {
        unsigned ch = (u8)*text++;
        int row, col;
        if (ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
        if (ch < 32 || ch > 95) ch = '?';
        for (row = 0; row < 7; row++) {
            for (col = 0; col < 5; col++) {
                if (sFont[ch - 32][col] & (1 << row)) PspOverlay_Rect(x + col, y + row, 1, 1, color);
            }
        }
        x += 6;
    }
#endif
}

void PspOverlay_End(void) {
#if PSP_GFX_BACKEND_GU
    if (sGuDrawing) {
        PspOverlayGu_End();
        sGuDrawing = 0;
        return;
    }
#endif
#if PSP_GFX_BACKEND_PSPGL
    if (!PspGfxBackend_IsGu()) {
        overlay_flush();
        return;
    }
#endif
#if PSP_GFX_BACKEND_GU || !PSP_GFX_BACKEND_PSPGL
    __sync_synchronize();
#endif
}

static u8 sMenuFontPixels[256 * 128 / 2] __attribute__((aligned(64)));
static int sMenuFontReady;
#if PSP_GFX_BACKEND_PSPGL
static GLuint sMenuFontTexture;
#endif

void PspOverlay_MenuBegin(void) {
    if (!sMenuFontReady) {
        unsigned source, pixel = 0;
        for (source = 0; source < sizeof(sMenuFontRle); source++) {
            unsigned run = (sMenuFontRle[source] >> 4) + 1;
            unsigned alpha = sMenuFontRle[source] & 15;
            while (run--) {
                if (pixel & 1) sMenuFontPixels[pixel / 2] |= alpha << 4;
                else sMenuFontPixels[pixel / 2] = alpha;
                pixel++;
            }
        }
        sMenuFontReady = 1;
    }
#if PSP_GFX_BACKEND_GU
    if (sGuDrawing) PspOverlayGu_MenuFont(sMenuFontPixels);
#endif
#if PSP_GFX_BACKEND_PSPGL
    if (!PspGfxBackend_IsGu()) {
    overlay_flush();
    if (!sMenuFontTexture) {
        static u16 pixels[256 * 128];
        unsigned i;
        for (i = 0; i < 256 * 128; i++) {
            unsigned alpha = (sMenuFontPixels[i / 2] >> ((i & 1) * 4)) & 15;
            pixels[i] = 0xFFF0 | alpha;
        }
        glGenTextures(1, &sMenuFontTexture);
        glBindTexture(GL_TEXTURE_2D, sMenuFontTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 128, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, pixels);
    } else {
        glBindTexture(GL_TEXTURE_2D, sMenuFontTexture);
    }
    }
#endif
}

int PspOverlay_MenuTextWidth(const char* text) {
    int width = 0;
    while (*text) {
        unsigned ch = (u8)*text++;
        if (ch < 32 || ch > 126) ch = '?';
        width += sMenuGlyphs[ch - 32].advance;
    }
    return width;
}

static void overlay_menu_glyph(int x, int y, unsigned ch, u32 color) {
    const PspMenuGlyph* glyph = &sMenuGlyphs[ch - 32];
    int left = x + glyph->left;
    int top = y + glyph->top;
    if (!glyph->width || !glyph->height) return;
#if PSP_GFX_BACKEND_GU
    if (sGuDrawing) {
        PspOverlayGu_Glyph(left, top, (ch - 32) % 16 * 16, (ch - 32) / 16 * 16,
                           glyph->width, glyph->height, color, 1);
        return;
    }
#endif
#if PSP_GFX_BACKEND_PSPGL
    if (!PspGfxBackend_IsGu()) {
    {
        float u = ((ch - 32) % 16 * 16) / 256.0f;
        float v = ((ch - 32) / 16 * 16) / 128.0f;
        float u1 = u + glyph->width / 256.0f;
        float v1 = v + glyph->height / 128.0f;
        int right = left + glyph->width;
        int bottom = top + glyph->height;
        OverlayVertex vertices[6] = {
            {u,v,color,left,top,0}, {u1,v,color,right,top,0}, {u,v1,color,left,bottom,0},
            {u,v1,color,left,bottom,0}, {u1,v,color,right,top,0}, {u1,v1,color,right,bottom,0}
        };
        int i;
        overlay_texture(1);
        if (sCount + 6 > 768) overlay_flush();
        for (i = 0; i < 6; i++) sVertices[sCount++] = vertices[i];
    }
        return;
    }
#endif
#if PSP_GFX_BACKEND_GU || !PSP_GFX_BACKEND_PSPGL
    {
        int row, column;
        int origin = (ch - 32) / 16 * 16 * 256 + (ch - 32) % 16 * 16;
        for (row = 0; row < glyph->height; row++) {
            for (column = 0; column < glyph->width;) {
                int pixel = origin + row * 256 + column;
                unsigned coverage = (sMenuFontPixels[pixel / 2] >> ((pixel & 1) * 4)) & 15;
                int run = 1;
                if (!coverage) { column++; continue; }
                while (column + run < glyph->width) {
                    int next = pixel + run;
                    unsigned alpha = (sMenuFontPixels[next / 2] >> ((next & 1) * 4)) & 15;
                    if (alpha != coverage) break;
                    run++;
                }
                PspOverlay_Rect(left + column, top + row, run, 1,
                                (color & 0xFFFFFFU) | (((color >> 24) * coverage / 15) << 24));
                column += run;
            }
        }
    }
#endif
}

void PspOverlay_MenuText(int x, int y, u32 color, const char* text) {
    int pass;
    for (pass = 0; pass < 2; pass++) {
        const char* next = text;
        int pen = x;
        while (*next) {
            unsigned ch = (u8)*next++;
            if (ch < 32 || ch > 126) ch = '?';
            overlay_menu_glyph(pen, y + (pass == 0), ch, pass == 0 ? 0x80000000U : color);
            pen += sMenuGlyphs[ch - 32].advance;
        }
    }
}

int PspOverlay_BeginMenuFrame(void) {
#if PSP_GFX_BACKEND_GU
    if (PspGfxBackend_IsGu()) {
        sGuDrawing = PspOverlayGu_Begin(sFont);
        return sGuDrawing;
    }
#endif
    return PspOverlay_Begin();
}

void PspOverlay_MenuEnd(void) {
#if PSP_GFX_BACKEND_PSPGL
    if (!PspGfxBackend_IsGu()) {
        overlay_flush();
        glBindTexture(GL_TEXTURE_2D, sFontTexture);
    }
#endif
}

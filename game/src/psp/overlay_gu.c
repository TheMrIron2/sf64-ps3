#include <pspgu.h>
#include <pspkernel.h>
#include <stddef.h>

#include "src/psp/overlay_gu.h"
#include "src/psp/gfx/gfx_psp_gu_device.h"

#define OVERLAY_VERTICES 128
#define OVERLAY_LIST_RESERVE (128 * 1024)

typedef struct {
    u16 u, v;
    u32 color;
    short x, y, z, padding;
} OverlayGuVertex;

typedef char OverlayGuVertexSize[(sizeof(OverlayGuVertex) == 16) ? 1 : -1];

static u32 sPalette[16] __attribute__((aligned(64)));
static u8 sHudPixels[128 * 32 / 2] __attribute__((aligned(64)));
static const u8* sMenuPixels;
static OverlayGuVertex* sVertices;
static int sCount;
static int sMode;
static int sFontsReady;

static void overlay_gu_flush(void) {
    if (!sCount) return;
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
                   sCount, NULL, sVertices);
    sCount = 0;
    sVertices = NULL;
}

static void overlay_gu_mode(int mode) {
    if (mode == sMode) return;
    overlay_gu_flush();
    if (mode) {
        sceGuEnable(GU_TEXTURE_2D);
        sceGuTexImage(0, mode == 1 ? 256 : 128, mode == 1 ? 128 : 32,
                      mode == 1 ? 256 : 128, mode == 1 ? sMenuPixels : sHudPixels);
        sceGuTexFlush();
    } else {
        sceGuDisable(GU_TEXTURE_2D);
    }
    sMode = mode;
}

static void overlay_gu_sprite(int x, int y, int u, int v, int width, int height, u32 color) {
    OverlayGuVertex* vertices;
    if (sCount + 2 > OVERLAY_VERTICES) overlay_gu_flush();
    if (!sVertices) sVertices = sceGuGetMemory(sizeof(OverlayGuVertex) * OVERLAY_VERTICES);
    vertices = &sVertices[sCount];
    vertices[0] = (OverlayGuVertex){ u, v, color, x, y, 0, 0 };
    vertices[1] = (OverlayGuVertex){ u + width, v + height, color, x + width, y + height, 0, 0 };
    sCount += 2;
}

int PspOverlayGu_Begin(const u8 font[64][5]) {
    int i, x, y;
    if (!PspGfxGuDevice_CanAppendOverlay(OVERLAY_LIST_RESERVE)) return 0;
    if (!sFontsReady) {
        for (i = 0; i < 16; i++) sPalette[i] = 0x00FFFFFFU | ((u32)(i * 17) << 24);
        for (i = 0; i < 64; i++) {
            for (y = 0; y < 7; y++) {
                for (x = 0; x < 5; x++) {
                    int pixel = (i / 16 * 8 + y) * 128 + i % 16 * 8 + x;
                    if (font[i][x] & (1 << y)) sHudPixels[pixel / 2] |= 15 << ((pixel & 1) * 4);
                }
            }
        }
        sceKernelDcacheWritebackRange(sPalette, sizeof(sPalette));
        sceKernelDcacheWritebackRange(sHudPixels, sizeof(sHudPixels));
        sFontsReady = 1;
    }
    sCount = 0;
    sVertices = NULL;
    sMode = -1;
    sceGuOffset(2048 - 240, 2048 - 136);
    sceGuViewport(2048, 2048, 480, 272);
    sceGuScissor(0, 0, 480, 272);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_FOG);
    sceGuDisable(GU_LIGHTING);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_CLIP_PLANES);
    sceGuDepthMask(GU_TRUE);
    sceGuPixelMask(0);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuShadeModel(GU_SMOOTH);
    sceGuTexMode(GU_PSM_T4, 0, 0, 0);
    sceGuClutMode(GU_PSM_8888, 0, 15, 0);
    sceGuClutLoad(2, sPalette);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexMapMode(GU_TEXTURE_COORDS, 0, 0);
    sceGuTexLevelMode(GU_TEXTURE_CONST, 0.0f);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    return 1;
}

void PspOverlayGu_Rect(int x, int y, int width, int height, u32 color) {
    overlay_gu_mode(0);
    overlay_gu_sprite(x, y, 0, 0, width, height, color);
}

void PspOverlayGu_MenuFont(const u8* pixels) {
    if (sMenuPixels != pixels) {
        sMenuPixels = pixels;
        sceKernelDcacheWritebackRange(pixels, 256 * 128 / 2);
    }
}

void PspOverlayGu_Glyph(int x, int y, int u, int v, int width, int height, u32 color, int menu) {
    overlay_gu_mode(menu ? 1 : 2);
    overlay_gu_sprite(x, y, u, v, width, height, color);
}

void PspOverlayGu_End(void) {
    overlay_gu_flush();
    // The next device and backend BeginFrame restore and invalidate game state
}

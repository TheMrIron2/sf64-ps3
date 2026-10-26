#include "global.h"
#include "src/psp/aquas_particles.h"
#include "src/psp/renderer.h"

int PspAquasParticles_TryEmit(float scale, int index, int alpha) {
#if PSP_FAST_AQUAS && PSP_GFX_BACKEND_GU
    Gfx* start;
    Gfx matrix;
    if (!PspSettings_UseFastGraphics()) return 0;
    start = gMasterDisp - 1;
    PSP_RENDERER_DL_MTXF(&matrix, NULL, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    if (start->words.w0 != matrix.words.w0) return 0;
    Graphics_SetScaleMtx(scale);
    matrix = gMasterDisp[-1];
    gDPNoOpTag(start, PSP_AQUAS_PARTICLE_MARKER);
    start[1].words.w0 = matrix.words.w1;
    start[1].words.w1 = ((u32) (index & 3) << 8) | ((u32) alpha & 255u);
    gMasterDisp = start + 2;
    return 1;
#else
    (void) scale; (void) index; (void) alpha;
    return 0;
#endif
}

#include "src/psp/gfx/gfx_psp_backend.h"
#include "src/psp/gfx/gfx_psp_backend_impl.h"

#if PSP_GFX_BACKEND_GU
static const PspGfxBackendOps sGuOperations = {
    PspGfxBackendGu_TextureDebugId,
    PspGfxBackendGu_TextureDebugGeneration,
    PspGfxBackendGu_TextureSupported,
    PspGfxBackendGu_FindTexture,
    PspGfxBackendGu_CreateTexture,
    PspGfxBackendGu_InvalidateRgba16Texture,
    PspGfxBackendGu_SetScissor,
    PspGfxBackendGu_ClearScissor,
    PspGfxBackendGu_DrawTriangles,
    PspGfxBackendGu_ReserveVertices,
    PspGfxBackendGu_DrawReservedTriangles,
    PspGfxBackendGu_DrawFogTriangles,
    PspGfxBackendGu_DrawSprites,
    PspGfxBackendGu_DrawSolidRect,
    PspGfxBackendGu_SetHudAnchor,
    PspGfxBackendGu_BeginReplayCache,
    PspGfxBackendGu_EndReplayCache,
    PspGfxBackendGu_ReplayCacheReady,
    PspGfxBackendGu_ReplayCacheInvalidate,
    PspGfxBackendGu_ReplayCache,
};
#endif

#if PSP_GFX_BACKEND_PSPGL
static const PspGfxBackendOps sPspglOperations = {
    PspGfxBackendPspgl_TextureDebugId,
    PspGfxBackendPspgl_TextureDebugGeneration,
    PspGfxBackendPspgl_TextureSupported,
    PspGfxBackendPspgl_FindTexture,
    PspGfxBackendPspgl_CreateTexture,
    PspGfxBackendPspgl_InvalidateRgba16Texture,
    PspGfxBackendPspgl_SetScissor,
    PspGfxBackendPspgl_ClearScissor,
    PspGfxBackendPspgl_DrawTriangles,
    PspGfxBackendPspgl_ReserveVertices,
    PspGfxBackendPspgl_DrawReservedTriangles,
    PspGfxBackendPspgl_DrawFogTriangles,
    PspGfxBackendPspgl_DrawSprites,
    PspGfxBackendPspgl_DrawSolidRect,
    PspGfxBackendPspgl_SetHudAnchor,
    PspGfxBackendPspgl_BeginReplayCache,
    PspGfxBackendPspgl_EndReplayCache,
    PspGfxBackendPspgl_ReplayCacheReady,
    PspGfxBackendPspgl_ReplayCacheInvalidate,
    PspGfxBackendPspgl_ReplayCache,
};
#endif

#if PSP_GFX_BACKEND_GU
const PspGfxBackendOps* gPspGfxBackendOps = &sGuOperations;
PspGfxBackendKind gPspGfxBackendKind = PSP_GFX_KIND_GU;
#else
const PspGfxBackendOps* gPspGfxBackendOps = &sPspglOperations;
PspGfxBackendKind gPspGfxBackendKind = PSP_GFX_KIND_PSPGL;
#endif

static int sSelectionLocked;

int PspGfxBackend_Select(PspGfxBackendKind backend) {
    if (sSelectionLocked) {
        return backend == gPspGfxBackendKind;
    }

    if (backend == PSP_GFX_KIND_GU) {
#if PSP_GFX_BACKEND_GU
        gPspGfxBackendOps = &sGuOperations;
        gPspGfxBackendKind = backend;
        sSelectionLocked = 1;
        return 1;
#else
        return 0;
#endif
    }
    if (backend == PSP_GFX_KIND_PSPGL) {
#if PSP_GFX_BACKEND_PSPGL
        gPspGfxBackendOps = &sPspglOperations;
        gPspGfxBackendKind = backend;
        sSelectionLocked = 1;
        return 1;
#else
        return 0;
#endif
    }
    return 0;
}

void PspGfxBackend_LockSelection(void) {
    sSelectionLocked = 1;
}

PspGfxBackendKind PspGfxBackend_GetActive(void) {
    return gPspGfxBackendKind;
}

PspGfxBackendKind PspGfxBackend_GetBuildDefault(void) {
#if PSP_GFX_BACKEND_GU
    return PSP_GFX_KIND_GU;
#else
    return PSP_GFX_KIND_PSPGL;
#endif
}

const char* PspGfxBackend_GetName(void) {
    return gPspGfxBackendKind == PSP_GFX_KIND_GU ? "GU" : "PSPGL";
}

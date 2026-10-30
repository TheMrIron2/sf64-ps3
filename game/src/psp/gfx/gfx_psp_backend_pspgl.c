#include "src/psp/gfx/gfx_psp_backend.h"
#include "src/psp/gfx/gfx_psp_backend_impl.h"

#include "src/psp/gfx/gfx_pspgl.h"

#include <stdint.h>

static PspGfxTextureHandle psp_gfx_backend_texture(u32 textureId, PspGfxPspglTextureRef textureRef) {
    PspGfxTextureHandle handle;

    if (textureId == 0) {
        return PspGfxTextureHandle_Null();
    }

    handle.opaque[0] = textureId;
    handle.opaque[1] = (u32) (uintptr_t) textureRef.state;
    handle.opaque[2] = textureRef.texture;
    handle.opaque[3] = textureRef.generation;
    return handle;
}

static u32 psp_gfx_backend_texture_id(PspGfxTextureHandle handle) {
    return handle.opaque[0];
}

static PspGfxPspglTextureRef psp_gfx_backend_texture_ref(PspGfxTextureHandle handle) {
    PspGfxPspglTextureRef textureRef = { 0 };

    textureRef.state = (PspGfxPspglTextureParameterState*) (uintptr_t) handle.opaque[1];
    textureRef.texture = handle.opaque[2];
    textureRef.generation = handle.opaque[3];
    return textureRef;
}

static PspGfxPspglTextureEnv psp_gfx_backend_texture_env(PspGfxTextureEnv textureEnv) {
    switch (textureEnv) {
        case PSP_GFX_TEX_MODULATE:
            return PSP_GFX_PSPGL_TEX_MODULATE;
        case PSP_GFX_TEX_BLEND:
            return PSP_GFX_PSPGL_TEX_BLEND;
        case PSP_GFX_TEX_REPLACE:
        default:
            return PSP_GFX_PSPGL_TEX_REPLACE;
    }
}

static PspGfxPspglTextureWrap psp_gfx_backend_texture_wrap(PspGfxTextureWrap wrap) {
    switch (wrap) {
        case PSP_GFX_WRAP_REPEAT:
            return PSP_GFX_PSPGL_WRAP_REPEAT;
        case PSP_GFX_WRAP_MIRROR:
            return PSP_GFX_PSPGL_WRAP_MIRROR;
        case PSP_GFX_WRAP_CLAMP:
        default:
            return PSP_GFX_PSPGL_WRAP_CLAMP;
    }
}

static PspGfxPspglVertexReservation psp_gfx_backend_reservation(const PspGfxVertexReservation* reservation) {
    PspGfxPspglVertexReservation pspglReservation;

    pspglReservation.vertices = reservation->vertices;
    pspglReservation.firstVertex = reservation->token.opaque;
    pspglReservation.capacity = reservation->capacity;
    return pspglReservation;
}

static void psp_gfx_backend_draw_state(const PspGfxDrawState* state, u32* textureId,
                                       PspGfxPspglTextureRef* textureRef) {
    *textureId = psp_gfx_backend_texture_id(state->texture);
    *textureRef = psp_gfx_backend_texture_ref(state->texture);
}

u32 PspGfxBackendPspgl_TextureDebugId(PspGfxTextureHandle handle) {
    return handle.opaque[0];
}

u32 PspGfxBackendPspgl_TextureDebugGeneration(PspGfxTextureHandle handle) {
    return handle.opaque[3];
}

int PspGfxBackendPspgl_TextureSupported(const PspGfxTextureRequest* request) {
    return PspGfxPspgl_TextureSupported(request);
}

void PspGfxBackendPspgl_InvalidateRgba16Texture(const u16* pixels) {
    PspGfxPspgl_InvalidateRgba16Texture(pixels);
}

void PspGfxBackendPspgl_SetScissor(float ulx, float uly, float lrx, float lry) {
    PspGfxPspgl_SetScissor(ulx, uly, lrx, lry);
}

void PspGfxBackendPspgl_ClearScissor(void) {
    PspGfxPspgl_ClearScissor();
}

int PspGfxBackendPspgl_FindTexture(const PspGfxTextureRequest* request, PspGfxTextureResult* result) {
    PspGfxPspglTextureRef textureRef = { 0 };
    u32 textureId = 0;

    *result = (PspGfxTextureResult) { 0 };
    if (!PspGfxPspgl_FindTexture(request, &textureId, &textureRef, &result->uploadWidth, &result->uploadHeight,
                                 &result->uploadX, &result->uploadY)) {
        result->cacheResult = PSP_GFX_TEXTURE_CACHE_MISS;
        return 0;
    }
    result->handle = psp_gfx_backend_texture(textureId, textureRef);
    result->cacheResult = PSP_GFX_TEXTURE_CACHE_HIT;
    return 1;
}

int PspGfxBackendPspgl_CreateTexture(const PspGfxTextureRequest* request, PspGfxTextureResult* result) {
    PspGfxPspglTextureRef textureRef = { 0 };
    u32 textureId;

    *result = (PspGfxTextureResult) { 0 };
    textureId = PspGfxPspgl_CreateTexture(request, &textureRef, &result->uploadWidth, &result->uploadHeight,
                                          &result->uploadX, &result->uploadY);
    result->handle = psp_gfx_backend_texture(textureId, textureRef);
    result->cacheResult = textureId != 0 ? PSP_GFX_TEXTURE_CACHE_CREATED : PSP_GFX_TEXTURE_CACHE_FAILED;
    return textureId != 0;
}

void PspGfxBackendPspgl_DrawTriangles(const PspGfxVertex* vertices, u32 vertexCount, const PspGfxDrawState* state) {
    PspGfxPspglTextureRef textureRef = { 0 };
    u32 textureId;

    psp_gfx_backend_draw_state(state, &textureId, &textureRef);
    PspGfxPspgl_DrawColoredTriangles(
        vertices, vertexCount, textureId, textureRef, psp_gfx_backend_texture_env(state->textureEnv),
        state->textureEnvColor, psp_gfx_backend_texture_wrap(state->wrapS), psp_gfx_backend_texture_wrap(state->wrapT),
        state->alphaTest, state->blend, state->premultiplied, state->depthTest, state->depthWrite, state->fog,
        state->fogColor, state->fogStart, state->fogEnd, state->projectionMatrix, state->projectionSerial,
        state->pretransformed, state->pointFilter, state->viewport);
}

int PspGfxBackendPspgl_ReserveVertices(u32 vertexCapacity, PspGfxVertexReservation* reservation) {
    PspGfxPspglVertexReservation pspglReservation;

    if (!PspGfxPspgl_ReserveColoredVertices(vertexCapacity, &pspglReservation)) {
        return 0;
    }
    reservation->vertices = pspglReservation.vertices;
    reservation->token.opaque = pspglReservation.firstVertex;
    reservation->capacity = pspglReservation.capacity;
    return 1;
}

void PspGfxBackendPspgl_DrawReservedTriangles(const PspGfxVertexReservation* reservation, u32 vertexCount,
                                         const PspGfxDrawState* state) {
    PspGfxPspglVertexReservation pspglReservation = psp_gfx_backend_reservation(reservation);
    PspGfxPspglTextureRef textureRef = { 0 };
    u32 textureId;

    psp_gfx_backend_draw_state(state, &textureId, &textureRef);
    PspGfxPspgl_DrawReservedColoredTriangles(
        &pspglReservation, vertexCount, textureId, textureRef, psp_gfx_backend_texture_env(state->textureEnv),
        state->textureEnvColor, psp_gfx_backend_texture_wrap(state->wrapS), psp_gfx_backend_texture_wrap(state->wrapT),
        state->alphaTest, state->blend, state->premultiplied, state->depthTest, state->depthWrite, state->fog,
        state->fogColor, state->fogStart, state->fogEnd, state->projectionMatrix, state->projectionSerial,
        state->pretransformed, state->pointFilter, state->viewport);
}

void PspGfxBackendPspgl_DrawFogTriangles(const PspGfxFogVertex* vertices, u32 vertexCount, const PspGfxFogDrawState* state) {
    PspGfxPspgl_DrawFogTriangles(vertices, vertexCount, state->projectionMatrix, state->projectionSerial,
                                 state->pretransformed, state->depthTest, state->viewport);
}

void PspGfxBackendPspgl_DrawSprites(const PspGfxVertex* vertices, u32 vertexCount, const PspGfxDrawState* state) {
    PspGfxPspglTextureRef textureRef = { 0 };
    u32 textureId;

    psp_gfx_backend_draw_state(state, &textureId, &textureRef);
    PspGfxPspgl_DrawColoredSprites(
        vertices, vertexCount, textureId, textureRef, psp_gfx_backend_texture_env(state->textureEnv),
        state->textureEnvColor, psp_gfx_backend_texture_wrap(state->wrapS), psp_gfx_backend_texture_wrap(state->wrapT),
        state->alphaTest, state->blend, state->premultiplied, state->depthTest, state->depthWrite, state->fog,
        state->fogColor, state->fogStart, state->fogEnd, state->projectionMatrix, state->projectionSerial,
        state->pretransformed, state->pointFilter, state->viewport);
}

void PspGfxBackendPspgl_DrawSolidRect(float ulx, float uly, float lrx, float lry, u32 color, int blend,
                                 PspGfxViewportPolicy viewport) {
    PspGfxPspgl_DrawSolidRect(ulx, uly, lrx, lry, color, blend, viewport);
}

void PspGfxBackendPspgl_SetHudAnchor(s16 x, s16 y) {
    PspGfxPspgl_SetHudAnchor(x, y);
}

void PspGfxBackendPspgl_BeginReplayCache(void) {
    PspGfxPspgl_BeginReplayCache();
}

void PspGfxBackendPspgl_EndReplayCache(void) {
    PspGfxPspgl_EndReplayCache();
}

int PspGfxBackendPspgl_ReplayCacheReady(void) {
    return PspGfxPspgl_ReplayCacheReady();
}

void PspGfxBackendPspgl_ReplayCacheInvalidate(void) {
    PspGfxPspgl_ReplayCacheInvalidate();
}

void PspGfxBackendPspgl_ReplayCache(void) {
    PspGfxPspgl_ReplayCache();
}

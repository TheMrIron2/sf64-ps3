#ifndef PSP_GFX_BACKEND_IMPL_H
#define PSP_GFX_BACKEND_IMPL_H

#include "src/psp/gfx/gfx_psp_backend.h"

#if PSP_GFX_BACKEND_GU
u32 PspGfxBackendGu_TextureDebugId(PspGfxTextureHandle handle);
u32 PspGfxBackendGu_TextureDebugGeneration(PspGfxTextureHandle handle);
int PspGfxBackendGu_TextureSupported(const PspGfxTextureRequest* request);
int PspGfxBackendGu_FindTexture(const PspGfxTextureRequest* request, PspGfxTextureResult* result);
int PspGfxBackendGu_CreateTexture(const PspGfxTextureRequest* request, PspGfxTextureResult* result);
void PspGfxBackendGu_InvalidateRgba16Texture(const u16* pixels);
void PspGfxBackendGu_SetScissor(float ulx, float uly, float lrx, float lry);
void PspGfxBackendGu_ClearScissor(void);
void PspGfxBackendGu_DrawTriangles(const PspGfxVertex* vertices, u32 vertexCount, const PspGfxDrawState* state);
int PspGfxBackendGu_ReserveVertices(u32 vertexCapacity, PspGfxVertexReservation* reservation);
void PspGfxBackendGu_DrawReservedTriangles(const PspGfxVertexReservation* reservation, u32 vertexCount,
                                           const PspGfxDrawState* state);
void PspGfxBackendGu_DrawFogTriangles(const PspGfxFogVertex* vertices, u32 vertexCount,
                                      const PspGfxFogDrawState* state);
void PspGfxBackendGu_DrawSprites(const PspGfxVertex* vertices, u32 vertexCount, const PspGfxDrawState* state);
void PspGfxBackendGu_DrawSolidRect(float ulx, float uly, float lrx, float lry, u32 color, int blend,
                                   PspGfxViewportPolicy viewport);
void PspGfxBackendGu_SetHudAnchor(s16 x, s16 y);
void PspGfxBackendGu_BeginReplayCache(void);
void PspGfxBackendGu_EndReplayCache(void);
int PspGfxBackendGu_ReplayCacheReady(void);
void PspGfxBackendGu_ReplayCacheInvalidate(void);
void PspGfxBackendGu_ReplayCache(void);
#endif

#if PSP_GFX_BACKEND_PSPGL
u32 PspGfxBackendPspgl_TextureDebugId(PspGfxTextureHandle handle);
u32 PspGfxBackendPspgl_TextureDebugGeneration(PspGfxTextureHandle handle);
int PspGfxBackendPspgl_TextureSupported(const PspGfxTextureRequest* request);
int PspGfxBackendPspgl_FindTexture(const PspGfxTextureRequest* request, PspGfxTextureResult* result);
int PspGfxBackendPspgl_CreateTexture(const PspGfxTextureRequest* request, PspGfxTextureResult* result);
void PspGfxBackendPspgl_InvalidateRgba16Texture(const u16* pixels);
void PspGfxBackendPspgl_SetScissor(float ulx, float uly, float lrx, float lry);
void PspGfxBackendPspgl_ClearScissor(void);
void PspGfxBackendPspgl_DrawTriangles(const PspGfxVertex* vertices, u32 vertexCount,
                                      const PspGfxDrawState* state);
int PspGfxBackendPspgl_ReserveVertices(u32 vertexCapacity, PspGfxVertexReservation* reservation);
void PspGfxBackendPspgl_DrawReservedTriangles(const PspGfxVertexReservation* reservation, u32 vertexCount,
                                              const PspGfxDrawState* state);
void PspGfxBackendPspgl_DrawFogTriangles(const PspGfxFogVertex* vertices, u32 vertexCount,
                                         const PspGfxFogDrawState* state);
void PspGfxBackendPspgl_DrawSprites(const PspGfxVertex* vertices, u32 vertexCount,
                                    const PspGfxDrawState* state);
void PspGfxBackendPspgl_DrawSolidRect(float ulx, float uly, float lrx, float lry, u32 color, int blend,
                                      PspGfxViewportPolicy viewport);
void PspGfxBackendPspgl_SetHudAnchor(s16 x, s16 y);
void PspGfxBackendPspgl_BeginReplayCache(void);
void PspGfxBackendPspgl_EndReplayCache(void);
int PspGfxBackendPspgl_ReplayCacheReady(void);
void PspGfxBackendPspgl_ReplayCacheInvalidate(void);
void PspGfxBackendPspgl_ReplayCache(void);
#endif

#endif

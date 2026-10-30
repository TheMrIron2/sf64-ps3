#include "src/psp/gfx/gfx_psp_backend.h"
#include "src/psp/gfx/gfx_psp_device.h"
#include "src/psp/gfx/gfx_psp_device_impl.h"

int PspGfxDevice_Init(void) {
    PspGfxBackend_LockSelection();
    if (PspGfxBackend_IsGu()) {
#if PSP_GFX_BACKEND_GU
        return PspGfxDeviceGu_Init();
#else
        return 0;
#endif
    }
#if PSP_GFX_BACKEND_PSPGL
    return PspGfxDevicePspgl_Init();
#else
    return 0;
#endif
}

int PspGfxDevice_IsReady(void) {
    if (PspGfxBackend_IsGu()) {
#if PSP_GFX_BACKEND_GU
        return PspGfxDeviceGu_IsReady();
#else
        return 0;
#endif
    }
#if PSP_GFX_BACKEND_PSPGL
    return PspGfxDevicePspgl_IsReady();
#else
    return 0;
#endif
}

int PspGfxDevice_BeginFrame(void) {
    if (PspGfxBackend_IsGu()) {
#if PSP_GFX_BACKEND_GU
        return PspGfxDeviceGu_BeginFrame();
#else
        return 0;
#endif
    }
#if PSP_GFX_BACKEND_PSPGL
    return PspGfxDevicePspgl_BeginFrame();
#else
    return 0;
#endif
}

int PspGfxDevice_Submit(void) {
    if (PspGfxBackend_IsGu()) {
#if PSP_GFX_BACKEND_GU
        return PspGfxDeviceGu_Submit();
#else
        return 0;
#endif
    }
#if PSP_GFX_BACKEND_PSPGL
    return PspGfxDevicePspgl_Submit();
#else
    return 0;
#endif
}

int PspGfxDevice_Present(void) {
    if (PspGfxBackend_IsGu()) {
#if PSP_GFX_BACKEND_GU
        return PspGfxDeviceGu_Present();
#else
        return 0;
#endif
    }
#if PSP_GFX_BACKEND_PSPGL
    return PspGfxDevicePspgl_Present();
#else
    return 0;
#endif
}

void PspGfxDevice_Shutdown(void) {
    if (PspGfxBackend_IsGu()) {
#if PSP_GFX_BACKEND_GU
        PspGfxDeviceGu_Shutdown();
#endif
        return;
    }
#if PSP_GFX_BACKEND_PSPGL
    PspGfxDevicePspgl_Shutdown();
#endif
}

void* PspGfxDevice_GetOverlayFrameBuffer(int* stride, int* pixelFormat) {
    if (PspGfxBackend_IsGu()) {
#if PSP_GFX_BACKEND_GU
        return PspGfxDeviceGu_GetOverlayFrameBuffer(stride, pixelFormat);
#else
        return NULL;
#endif
    }
#if PSP_GFX_BACKEND_PSPGL
    return PspGfxDevicePspgl_GetOverlayFrameBuffer(stride, pixelFormat);
#else
    return NULL;
#endif
}

#include "src/psp/gfx/gfx_psp_device.h"

#include "src/psp/gfx/gfx_pspgl.h"
#include "src/psp/gfx/gfx_pspgl_device.h"

int PspGfxDevicePspgl_Init(void) {
    if (!PspGfxPspglDevice_Init()) {
        return 0;
    }

    PspGfxPspgl_Init();
    return 1;
}

int PspGfxDevicePspgl_IsReady(void) {
    return PspGfxPspglDevice_IsReady();
}

int PspGfxDevicePspgl_BeginFrame(void) {
    if (!PspGfxDevicePspgl_IsReady()) {
        return 0;
    }

    PspGfxPspglDevice_BeginFrame();
    PspGfxPspgl_BeginFrame();
    return 1;
}

int PspGfxDevicePspgl_Submit(void) {
    if (!PspGfxDevicePspgl_IsReady()) {
        return 0;
    }

    PspGfxPspgl_Flush();
    return 1;
}

int PspGfxDevicePspgl_Present(void) {
    if (!PspGfxDevicePspgl_IsReady()) {
        return 0;
    }

    return PspGfxPspglDevice_EndFrame();
}

void PspGfxDevicePspgl_Shutdown(void) {
    PspGfxPspglDevice_Shutdown();
}

void* PspGfxDevicePspgl_GetOverlayFrameBuffer(int* stride, int* pixelFormat) {
    return PspGfxPspglDevice_GetPresentedFrameBuffer(stride, pixelFormat);
}

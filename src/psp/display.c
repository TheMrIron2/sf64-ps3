#include <n64psp/display.h>

#include "src/psp/display.h"
#include "src/psp/gfx/gfx_psp_backend.h"

static int sReady;
// Preserve the pinned getter while keeping the accepted UI layout fixed
int gPspUiScalingEnabled;
static n64psp_display_config sDisplayConfig;

int PspDisplay_Init(void) {
    if (sReady) {
        return 1;
    }
    if (!n64psp_display_configure(&sDisplayConfig, N64PSP_DISPLAY_PSP_480X272)) {
        return 0;
    }
    sReady = 1;
    return 1;
}

const n64psp_display_config* PspDisplay_GetConfig(void) {
    return &sDisplayConfig;
}

float PspDisplay_GetProjectionAspect(void) {
    return sDisplayConfig.display_aspect;
}

int PspDisplay_IsWidescreen(void) {
    return sDisplayConfig.mode == N64PSP_DISPLAY_PSP_480X272;
}

float PspDisplay_GetUiScaleY(void) {
    return (float) sDisplayConfig.ui_viewport_height / (float) sDisplayConfig.logical_height;
}

float PspDisplay_UiFromLeft(float x) {
    return n64psp_ui_from_left(&sDisplayConfig, x);
}

int PspDisplay_IsUiScalingEnabled(void) {
    return gPspUiScalingEnabled;
}

int PspDisplay_SetMode(int mode) {
    if (mode < 0 || mode >= N64PSP_DISPLAY_PSP_MODE_COUNT) {
        return 0;
    }
    if (!sReady && !PspDisplay_Init()) {
        return 0;
    }
    if (sDisplayConfig.mode == mode) {
        return 0;
    }
    if (!n64psp_display_configure(&sDisplayConfig, mode)) {
        return 0;
    }
    PspGfxBackend_ReplayCacheInvalidate();
    return 1;
}

#include "src/psp/settings.h"
#include "src/psp/display.h"
#include "src/psp/frame_scheduler.h"
#include "src/psp/frame_interpolation.h"

static int sDisplayMode = -1;
static int sPresentationHz;
#if PSP_FPS_OVERLAY
static int sPerformanceVisible = 1;
static int sTogglePerformance;
#endif

void PspSettings_SetDisplayMode(n64psp_display_mode mode) {
    if (mode >= 0 && mode < N64PSP_DISPLAY_PSP_MODE_COUNT) {
        sDisplayMode = mode;
    }
}

void PspSettings_SetPresentationHz(int hz) {
    if (hz == 30 || hz == 60) {
        sPresentationHz = hz;
    }
}

int PspSettings_NeedsBoundary(void) {
    return sDisplayMode >= 0 || sPresentationHz != 0
#if PSP_FPS_OVERLAY
           || sTogglePerformance
#endif
           ;
}

// Called before simulation generation with the previous graphics task complete
void PspSettings_Apply(void) {
#if PSP_FPS_OVERLAY
    if (sTogglePerformance) {
        sPerformanceVisible = !sPerformanceVisible;
        sTogglePerformance = 0;
    }
#endif
    if (sDisplayMode >= 0) {
        if (PspDisplay_SetMode(sDisplayMode)) {
            PspFrameInterpolation_Reset();
        }
        sDisplayMode = -1;
    }
    if (sPresentationHz != 0) {
        PspFrameScheduler_SetPresentationHz(sPresentationHz);
        sPresentationHz = 0;
    }
}

int PspSettings_PerformanceVisible(void) {
#if PSP_FPS_OVERLAY
    return sPerformanceVisible;
#else
    return 0;
#endif
}

void PspSettings_TogglePerformance(void) {
#if PSP_FPS_OVERLAY
    sTogglePerformance = !sTogglePerformance;
#endif
}

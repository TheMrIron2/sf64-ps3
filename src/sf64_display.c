#include "sf64_ps3/display.h"

#if defined(N64PS3_PS3)
#include <string.h>
#include <sysutil/video.h>
#endif

float sf64_display_aspect(void)
{
#if defined(N64PS3_PS3)
    static float aspect = SF64_DISPLAY_REFERENCE_ASPECT;
    static int initialized;

    if (!initialized) {
        videoState state;
        videoResolution resolution;

        memset(&state, 0, sizeof(state));
        if (videoGetState(VIDEO_PRIMARY, 0, &state) == 0 &&
            videoGetResolution(state.displayMode.resolution, &resolution) == 0 &&
            resolution.width != 0U && resolution.height != 0U) {
            if (state.displayMode.aspect == VIDEO_ASPECT_16_9) {
                aspect = 16.0f / 9.0f;
            } else if (state.displayMode.aspect == VIDEO_ASPECT_4_3) {
                aspect = SF64_DISPLAY_REFERENCE_ASPECT;
            } else {
                aspect = (float)resolution.width / (float)resolution.height;
            }
            initialized = 1;
        }
    }
    return aspect;
#else
    return SF64_DISPLAY_REFERENCE_ASPECT;
#endif
}

void sf64_display_fit_4_3(float aspect, float *scale_x, float *scale_y)
{
    *scale_x = 1.0f;
    *scale_y = 1.0f;
    if (aspect > SF64_DISPLAY_REFERENCE_ASPECT) {
        *scale_x = SF64_DISPLAY_REFERENCE_ASPECT / aspect;
    } else if (aspect > 0.0f) {
        *scale_y = aspect / SF64_DISPLAY_REFERENCE_ASPECT;
    }
}

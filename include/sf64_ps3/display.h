#ifndef SF64_PS3_DISPLAY_H
#define SF64_PS3_DISPLAY_H

#define SF64_DISPLAY_LOGICAL_WIDTH 320.0f
#define SF64_DISPLAY_LOGICAL_HEIGHT 240.0f
#define SF64_DISPLAY_REFERENCE_ASPECT \
    (SF64_DISPLAY_LOGICAL_WIDTH / SF64_DISPLAY_LOGICAL_HEIGHT)

/* Physical display aspect, including anamorphic PS3 video modes. */
float sf64_display_aspect(void);

/* Fit the original centered 320x240 space into the full-resolution viewport.
 * Apply to projection X/Y once; widescreen leaves vertical FOV unchanged and
 * exposes more of the world on either side of the original 4:3 image. */
void sf64_display_fit_4_3(float aspect, float *scale_x, float *scale_y);

#endif

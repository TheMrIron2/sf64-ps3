#ifndef SF64_PS3_TITLE_UI_H
#define SF64_PS3_TITLE_UI_H

#include "n64ps3/gfx.h"

#include <stdint.h>

typedef struct Sf64TitleUiInfo {
    uint32_t logo_strip_count;
    uint32_t generated_command_count;
} Sf64TitleUiInfo;

/* Capture RGBA16 texture-rectangle calls made by original title draw code. */
void sf64_title_ui_reset(void);
void sf64_title_logo_rect(void *gfx_pointer,
                          void *texture_pointer,
                          uint32_t width,
                          uint32_t height,
                          float x,
                          float y,
                          float x_scale,
                          float y_scale);

/* Append the captured original logo strips to an already-built title task. */
int sf64_title_ui_append_logo(N64ps3GfxTask *task,
                              const uint8_t *title_segment,
                              uint32_t title_segment_size,
                              Sf64TitleUiInfo *info);

#endif

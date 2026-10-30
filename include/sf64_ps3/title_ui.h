#ifndef SF64_PS3_TITLE_UI_H
#define SF64_PS3_TITLE_UI_H

#include "n64ps3/gfx.h"

#include <stdint.h>

typedef struct Sf64TitleUiInfo {
    uint32_t logo_strip_count;
    uint32_t generated_command_count;
    uint32_t n64_logo_panel_count;
    uint32_t n64_logo_triangle_count;
    uint32_t n64_logo_opaque_texels;
    uint32_t n64_logo_position_valid;
    uint32_t press_start_drawn;
} Sf64TitleUiInfo;

/* Capture RGBA16 texture-rectangle calls made by original title draw code. */
void sf64_title_ui_reset(void);
const char *sf64_title_ui_last_error(void);
void sf64_title_logo_rect(void *gfx_pointer,
                          void *texture_pointer,
                          uint32_t width,
                          uint32_t height,
                          float x,
                          float y,
                          float x_scale,
                          float y_scale);
void sf64_title_ia8_rect(void *gfx_pointer,
                         void *texture_pointer,
                         uint32_t width,
                         uint32_t height,
                         float x,
                         float y,
                         float x_scale,
                         float y_scale);
void sf64_title_prim_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

/* Append the original 2D logo strips and movable 3D Nintendo 64 logo. */
int sf64_title_ui_append_logo(N64ps3GfxTask *task,
                              const uint8_t *title_segment,
                              uint32_t title_segment_size,
                              const float n64_logo_position[3],
                              Sf64TitleUiInfo *info);

#endif

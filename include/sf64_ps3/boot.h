#ifndef SF64_PS3_BOOT_H
#define SF64_PS3_BOOT_H

#include "n64ps3/gfx.h"
#include <stdint.h>

#define SF64_BOOT_LOGO_BYTES (128U * 74U)
#define SF64_BOOT_RDRAM_BYTES 0xb000U
#define SF64_OPENING_RDRAM_BYTES 0x2c000U

typedef struct Sf64BootState {
    int state;
    int next_state_timer;
    uint64_t logo_deadline_us;
    int save_check_pending;
} Sf64BootState;

typedef struct Sf64OpeningState {
    int scene_state;
    int timer1;
    int timer2;
    int fill_alpha;
    int show_sun;
    int logo_state;
    int timer3;
    int message_frame;
    int show_great_fox;
    int card_state;
    int camera_segment;
    int fade_mode;
    int finished;
    int phase;
    int team_counter, team_name_index, team_name_visible;
    int team_frames[4];
    int draw_mode;
    int fill_red, fill_green, fill_blue;
    struct { int draw; } corneria;
    float starfox_alpha;
    float in_alpha;
    float fade_in_scale;
    float fade_out_scale;
    float n64_slide;
    float logo_x, logo_y, logo_scale_x, logo_scale_y;
    float light_x, light_y, light_scale, light_scale2, logo_fade_out;
    int sun_glare_alpha;
    float sun_screen_x, sun_screen_y, sun_scale_x, sun_scale_y;
    float camera_xrot, camera_yrot, camera_weight, camera_speed;
    float star_speed;
    float camera_distance;
    float great_fox_position[3];
    float team_z_offset, passage_offset;
    float eye[3], at[3], up[3];
} Sf64OpeningState;

void sf64_boot_init(Sf64BootState *boot);
int sf64_boot_update(Sf64BootState *boot, uint64_t now_us);
int sf64_boot_build_task(N64ps3GfxTask *task, uint8_t *rdram, uint32_t size,
                         const uint8_t *logo, uint32_t logo_size, int visible);
void sf64_opening_init(Sf64OpeningState *state);
void sf64_opening_update(Sf64OpeningState *state);
int sf64_opening_build_task(N64ps3GfxTask *task, uint8_t *rdram, uint32_t size,
                            const uint8_t *title, uint32_t title_size,
                            const uint8_t *stars, uint32_t stars_size,
                            Sf64OpeningState *state,
                            const uint8_t *great_fox, uint32_t great_fox_size);

#endif

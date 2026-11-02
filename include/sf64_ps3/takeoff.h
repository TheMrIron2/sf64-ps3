#ifndef SF64_PS3_TAKEOFF_H
#define SF64_PS3_TAKEOFF_H

#include "n64ps3/gfx.h"

#include <stdint.h>

/* One Arwing on the Great Fox deck during Title_CsTakeOff. */
typedef struct Sf64TakeoffArwing {
    float position[3];
    float scale;
    float rotation_degrees[3];
    float cockpit_glass_xrot;
    float laser_guns_ypos;
    float vel;
    int timer;
    int draw;
    int draw_shadow;
    int engine_glow;
    int engine_particle;
    float engine_glow_scale;
} Sf64TakeoffArwing;

/* State carried by the original Title_CsTakeOff update/draw pair. */
typedef struct Sf64TakeoffState {
    int scene_state;
    int timer1;
    int timer3;
    int fill_alpha;
    int fill_red;
    int fill_green;
    int fill_blue;
    int card_state; /* 0 none, 1 ARWING card */
    int platform_visible;
    int star_count;
    float deck_offset_y;
    float deck_offset_z;
    float deck_shadow;
    float deck_launcher_zpos;
    float light_x;
    float light_y;
    float light_dir[3];
    float light1_r;
    float light1_g;
    float light1_b;
    int ambient_r;
    int ambient_g;
    int ambient_b;
    float eye[3];
    float at[3];
    float up[3];
    float cam_x;
    float cam_y;
    float cam_z;
    float cam_eye_dist;
    float cam_at_dist;
    float cam_xrot;
    float cam_yrot;
    float cam_weight;
    int cam_segment;
    float cam_speed;
    Sf64TakeoffArwing arwing[4];
    int finished;
} Sf64TakeoffState;

void sf64_takeoff_init(Sf64TakeoffState *state);
void sf64_takeoff_update(Sf64TakeoffState *state);

/* Build the Great Fox deck, launchers, four Arwings, and ARWING card. */
int sf64_takeoff_build_task(N64ps3GfxTask *task,
                            uint8_t *rdram,
                            uint32_t rdram_size,
                            const uint8_t *arwing_segment,
                            uint32_t arwing_segment_size,
                            const uint8_t *title_segment,
                            uint32_t title_segment_size,
                            const Sf64TakeoffState *state);

/* State carried by the original Title_CsTakeOffSpace update/draw pair. */
typedef struct Sf64SpaceState {
    int scene_state;
    int timer1;
    int timer3;
    int fill_alpha;
    int fill_mode;
    int star_count;
    float great_fox_position[3];
    float great_fox_rotation[3];
    float great_fox_scale;
    float corneria_position[3];
    float corneria_scale;
    int corneria_draw;
    float light_dir[3];
    int light1_r;
    int light1_g;
    int light1_b;
    int ambient_r;
    int ambient_g;
    int ambient_b;
    float eye[3];
    float at[3];
    float up[3];
    float cam_weight;
    int cam_segment;
    float cam_speed;
    Sf64TakeoffArwing arwing[4];
    int finished;
} Sf64SpaceState;

void sf64_space_init(Sf64SpaceState *state);
void sf64_space_update(Sf64SpaceState *state);

/* Build the Great Fox, Corneria, and four flying Arwings. */
int sf64_space_build_task(N64ps3GfxTask *task,
                          uint8_t *rdram,
                          uint32_t rdram_size,
                          const uint8_t *arwing_segment,
                          uint32_t arwing_segment_size,
                          const uint8_t *title_segment,
                          uint32_t title_segment_size,
                          const uint8_t *great_fox_segment,
                          uint32_t great_fox_segment_size,
                          const Sf64SpaceState *state);

#endif

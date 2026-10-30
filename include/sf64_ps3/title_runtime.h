#ifndef SF64_PS3_TITLE_RUNTIME_H
#define SF64_PS3_TITLE_RUNTIME_H

#include <stdint.h>

typedef struct Sf64OriginalTitleArwingPose {
    float position[3];
    float scale;
    float rotation_degrees[3];
} Sf64OriginalTitleArwingPose;

typedef struct Sf64OriginalTitleLighting {
    uint8_t directional_rgb[3];
    uint8_t ambient_rgb[3];
    float direction[3];
} Sf64OriginalTitleLighting;

/* Runs original SF64 title setup/update and title-screen draw-state dispatch. */
int sf64_original_title_logic_init(void);
int sf64_original_title_logic_update(void);
int sf64_original_title_logic_draw(void);
void sf64_original_title_logic_get_state(int32_t *game_state,
                                        int32_t *title_state,
                                        int32_t *cutscene_state,
                                        int32_t *main_controller);
int sf64_original_title_logic_get_arwing_pose(
    Sf64OriginalTitleArwingPose *pose);
int sf64_original_title_logic_get_64_logo_position(float position[3]);
int sf64_original_title_logic_get_team_lighting(
    Sf64OriginalTitleLighting *lighting);
int sf64_original_title_logic_get_team_model(int index,
                                             uint32_t *skeleton_address,
                                             uint32_t *limb_count,
                                             float position[3],
                                             float rotation_degrees[3],
                                             float *scale,
                                             float joint_rotation_degrees[32][3]);
void sf64_original_title_logic_set_controller(uint16_t buttons,
                                              int8_t stick_x,
                                              int8_t stick_y);

/* Resolve an original 32-bit N64 segmented pointer against build resources. */
void *sf64_original_segmented_to_virtual(uintptr_t address);

#endif

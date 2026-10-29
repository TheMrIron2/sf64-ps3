#ifndef SF64_PS3_TITLE_RUNTIME_H
#define SF64_PS3_TITLE_RUNTIME_H

#include <stdint.h>

typedef struct Sf64OriginalTitleArwingPose {
    float position[3];
    float scale;
    float rotation_degrees[3];
} Sf64OriginalTitleArwingPose;

/* Runs the original title setup/update functions from sf64-psp source. */
int sf64_original_title_logic_init(void);
int sf64_original_title_logic_update(void);
int sf64_original_title_logic_get_arwing_pose(
    Sf64OriginalTitleArwingPose *pose);

/* Resolve an original 32-bit N64 segmented pointer against build resources. */
void *sf64_original_segmented_to_virtual(uintptr_t address);

#endif

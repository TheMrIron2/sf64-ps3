#ifndef SF64_PS3_ARWING_H
#define SF64_PS3_ARWING_H

#include "n64ps3/gfx.h"

#define SF64_ARWING_SEGMENT_ID 3U
#define SF64_ARWING_SEGMENT_SIZE 0x00019df0U
#define SF64_ARWING_RDRAM_REQUIRED_SIZE 0x00030000U
#define SF64_TITLE_STARFIELD_RDRAM_OFFSET 0x0002a000U
#define SF64_TITLE_STARFIELD_RDRAM_SIZE 0x00006000U

typedef struct Sf64ArwingBuildInfo {
    u32 animation_frame;
    u32 skeleton_limb_count;
    u32 display_list_count;
    u32 title_star_count;
    u32 generated_command_count;
} Sf64ArwingBuildInfo;

typedef struct Sf64ArwingTitlePose {
    float position[3];
    float scale;
    float rotation_degrees[3];
} Sf64ArwingTitlePose;

/* Use state produced by the original Title_Screen_Setup/Update routines. */
void sf64_arwing_set_title_pose(const Sf64ArwingTitlePose *pose);

/* Build a title-pose task from the original US Rev 1 ast_arwing segment. */
int sf64_arwing_build_title_task(const u8 *segment,
                                 u32 segment_size,
                                 u8 *rdram,
                                 u32 rdram_size,
                                 N64ps3GfxTask *task,
                                 Sf64ArwingBuildInfo *info);

/* Build the same original Arwing task, calling an external N64 display list
 * for the title background. The list is copied into task RDRAM segment 4. */
int sf64_arwing_build_title_task_with_starfield(
    const u8 *segment,
    u32 segment_size,
    const u8 *starfield_display_list,
    u32 starfield_display_list_size,
    u32 title_star_count,
    u8 *rdram,
    u32 rdram_size,
    N64ps3GfxTask *task,
    Sf64ArwingBuildInfo *info);

#endif

#ifndef SF64_PS3_ARWING_H
#define SF64_PS3_ARWING_H

#include "n64ps3/gfx.h"

#define SF64_ARWING_SEGMENT_ID 3U
#define SF64_ARWING_SEGMENT_SIZE 0x00019df0U
#define SF64_ARWING_RDRAM_REQUIRED_SIZE 0x00030000U

typedef struct Sf64ArwingBuildInfo {
    u32 animation_frame;
    u32 skeleton_limb_count;
    u32 display_list_count;
    u32 title_star_count;
    u32 generated_command_count;
} Sf64ArwingBuildInfo;

/* Build a title-pose task from the original US Rev 1 ast_arwing segment. */
int sf64_arwing_build_title_task(const u8 *segment,
                                 u32 segment_size,
                                 u8 *rdram,
                                 u32 rdram_size,
                                 N64ps3GfxTask *task,
                                 Sf64ArwingBuildInfo *info);

#endif

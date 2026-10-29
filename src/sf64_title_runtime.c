#include "sf64_ps3/title_runtime.h"

#include "global.h"
#include "fox_title.h"

extern const u8 sf64_arwing_segment_data[];
extern const unsigned int sf64_arwing_segment_size;
extern const u8 sf64_title_segment_data[];
extern const unsigned int sf64_title_segment_size;

extern void Title_Screen_Update(void);
extern TitleArwingInfo sTitleArwing[4];

SaveFile gSaveFile;
OSContPad gControllerPress[4];
s32 gMainController;
s32 gGameFrameCount;
s32 gRadioState;
s32 sRandSeed1;
s32 sRandSeed2;
s32 sRandSeed3;
f32 gCsCamEyeX;
f32 gCsCamEyeY;
f32 gCsCamEyeZ;
f32 gCsCamAtX;
f32 gCsCamAtY;
f32 gCsCamAtZ;
s32 gFillScreenRed;
s32 gFillScreenGreen;
s32 gFillScreenBlue;
s32 gFillScreenAlpha;
DrawMode gDrawMode;

extern GameState gGameState;

static int s_title_logic_initialized;

static void sf64_title_zero(void *destination, size_t size)
{
    u8 *bytes = (u8 *)destination;
    size_t index;

    for (index = 0; index < size; index++) {
        bytes[index] = 0;
    }
}

void *sf64_original_segmented_to_virtual(uintptr_t address)
{
    u32 n64_address = (u32)address;
    u32 segment = n64_address >> 24;
    u32 offset = n64_address & 0x00ffffffU;

    if (segment == 6U && offset < sf64_title_segment_size) {
        return (void *)(sf64_title_segment_data + offset);
    }
    if (segment == 3U && offset < sf64_arwing_segment_size) {
        return (void *)(sf64_arwing_segment_data + offset);
    }
    return (void *)address;
}

int sf64_original_title_logic_init(void)
{
    sf64_title_zero(&gSaveFile, sizeof(gSaveFile));
    sf64_title_zero(gControllerPress, sizeof(gControllerPress));
    gMainController = 0;
    gGameFrameCount = 0;
    gRadioState = 0;
    gFillScreenRed = 0;
    gFillScreenGreen = 0;
    gFillScreenBlue = 0;
    gFillScreenAlpha = 0;
    gGameState = GSTATE_TITLE;

    /* The original game seeds these from its boot-time clock service. */
    sRandSeed1 = 1;
    sRandSeed2 = 29000;
    sRandSeed3 = 9876;

    s_title_logic_initialized = 1;
    return 0;
}

int sf64_original_title_logic_update(void)
{
    if (!s_title_logic_initialized && sf64_original_title_logic_init() != 0) {
        return -1;
    }

    Title_Screen_Update();
    gGameFrameCount++;
    return 0;
}

int sf64_original_title_logic_get_arwing_pose(
    Sf64OriginalTitleArwingPose *pose)
{
    const TitleArwingInfo *arwing;

    if (!pose || !s_title_logic_initialized) {
        return -1;
    }

    arwing = &sTitleArwing[TEAM_FOX];
    pose->position[0] = arwing->pos.x;
    pose->position[1] = arwing->pos.y;
    pose->position[2] = arwing->pos.z;
    pose->scale = arwing->scale;
    pose->rotation_degrees[0] = arwing->xRot;
    pose->rotation_degrees[1] = arwing->yRot;
    pose->rotation_degrees[2] = arwing->zRot;
    return 0;
}

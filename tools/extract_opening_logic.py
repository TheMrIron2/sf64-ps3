#!/usr/bin/env python3
"""Extract the first original opening-logo states for the boot harness."""

import argparse
from pathlib import Path
from extract_title_starfield_source import extract_function, extract_initializer


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    title = (args.source / "src/overlays/ovl_menu/fox_title.c").read_text()
    stdlib = (args.source / "src/engine/fox_std_lib.c").read_text()
    map_source = (args.source / "src/overlays/ovl_menu/fox_map.c").read_text()
    update = extract_function(title, "Title_CsGreatFoxTraveling_Update")
    begin = update.index("        case 100:")
    end = update.index("\n    }\n\n    Title_ScreenFade_Update", begin)
    cases = update[begin:end]
    smooth = extract_function(stdlib, "Math_SmoothStepToF").replace(
        "f32 Math_SmoothStepToF(", "static f32 opening_smooth_step("
    )
    cases = cases.replace("Math_SmoothStepToF(", "opening_smooth_step(")
    cases = cases.replace("Title_Cutscene_SetCamera(D_menu_801B8294,", "opening_camera_step(state, opening_camera,")
    cases = cases.replace("Title_GetCamRot(", "opening_get_cam_rot(state, ")
    cases = cases.replace("Title_SetCamUp(", "opening_set_cam_up(state, ")
    cases = cases.replace("sCutsceneState = TITLE_CS_TEAM_RUNNING;", "state->phase = 1;")
    camera_data = extract_initializer(map_source, r"^\s*CameraPoint\s+D_menu_801B6B40\s*\[\s*\]\s*=\s*\{")
    camera_data = camera_data.replace("CameraPoint D_menu_801B6B40", "static CameraPoint opening_camera")
    cases = cases.replace("D_menu_801B6B40", "opening_camera")
    camera_calc = extract_function(title, "Title_Camera_Calc").replace("void Title_Camera_Calc(", "static void opening_camera_calc(")
    camera_step = extract_function(title, "Title_Cutscene_SetCamera")
    camera_step = camera_step.replace("void Title_Cutscene_SetCamera(", "static void opening_camera_step(Sf64OpeningState *state, ")
    camera_step = camera_step.replace("Title_Camera_Calc(", "opening_camera_calc(")
    fade = extract_function(title, "Title_ScreenFade_Update").replace("void Title_ScreenFade_Update(void)", "static void opening_fade(Sf64OpeningState *state)")
    team_data = extract_initializer(title, r"^\s*CameraPoint\s+D_menu_801ADA94\s*\[\s*50\s*\]\s*=\s*\{")
    team_data = team_data.replace("CameraPoint D_menu_801ADA94", "static CameraPoint team_camera")
    team_update = extract_function(title, "Title_CsTeamRunning_Update")
    team_update = team_update.replace("void Title_CsTeamRunning_Update(void)", "static void opening_team_update(Sf64OpeningState *state)")
    team_update = team_update.replace("Title_CsTeamRunning_Setup()", "opening_team_setup(state)")
    team_update = team_update.replace("D_menu_801B8294 = D_menu_801ADA94;", "(void)0;")
    team_update = team_update.replace("Title_Cutscene_SetCamera(D_menu_801B8294,", "opening_camera_step(state, team_camera,")
    team_update = team_update.replace("Title_GetCamRot(", "opening_get_cam_rot(state, ")
    team_update = team_update.replace("Title_SetCamUp(", "opening_set_cam_up(state, ")
    team_update = team_update.replace("Math_SmoothStepToF(", "opening_smooth_step(")
    team_update = team_update.replace("Title_ScreenFade_Update()", "opening_fade(state)")
    team_update = team_update.replace("sCutsceneState = TITLE_GREAT_FOX_CLOSE_UP;", "state->phase = 2;")
    team_update = team_update.replace("sTimer3 == D_menu_801ADF44[D_menu_801B8340]", "D_menu_801B8340 < 4 && sTimer3 == D_menu_801ADF44[D_menu_801B8340]")
    header = '''#include "sf64_ps3/boot.h"
#include <string.h>
#include <math.h>
#include <stdbool.h>
#pragma GCC diagnostic ignored "-Wmissing-braces"
typedef float f32;
typedef struct { f32 x, y, z; } Vec3f;
typedef struct { Vec3f eye, at; } CameraPoint;
#define SQ(x) ((x) * (x))
#define CUBE(x) ((x) * (x) * (x))
#define sSceneState state->scene_state
#define sTimer1 state->timer1
#define sTimer2 state->timer2
#define sTimer3 state->timer3
#define gFillScreenAlpha state->fill_alpha
#define D_menu_801B7BD8 state->show_sun
#define D_menu_801B7BB8 state->n64_slide
#define sTitleLogoState state->logo_state
#define TITLE_LOGO_NINTENDO_64 1
#define TITLE_LOGO_NONE -1
#define TITLE_CARD_NONE 0
#define TITLE_CARD_GREAT_FOX 1
#define sTitleCardState state->card_state
#define sTitleMsgFrameCount state->message_frame
#define D_menu_801B8348 state->show_great_fox
#define D_menu_801B8298 state->camera_segment
#define D_menu_801B828C state->camera_weight
#define D_menu_801B8290 state->camera_speed
#define D_menu_801B82D4 state->light_scale
#define D_menu_801B82D8 state->light_scale2
#define D_menu_801B82DC state->logo_fade_out
#define D_menu_801B86C8 state->light_x
#define D_menu_801B86CC state->light_y
#define D_menu_801B86A8 state->camera_xrot
#define D_menu_801B86AC state->camera_yrot
#define D_menu_801B9070 state->logo_x
#define D_menu_801B9074 state->logo_y
#define D_menu_801B9078 state->logo_scale_x
#define D_menu_801B907C state->logo_scale_y
#define D_menu_801B7BE8 state->star_speed
#define D_menu_801B869C state->fade_mode
#define gCsCamEyeX state->eye[0]
#define gCsCamEyeY state->eye[1]
#define gCsCamEyeZ state->eye[2]
#define gCsCamAtX state->at[0]
#define gCsCamAtY state->at[1]
#define gCsCamAtZ state->at[2]
#define gFillScreenRed state->fill_red
#define gFillScreenGreen state->fill_green
#define gFillScreenBlue state->fill_blue
#define sTitleCorneria state->corneria
#define D_menu_801B7BEC state->team_counter
#define D_menu_801B8340 state->team_name_index
#define D_menu_801B84D0 state->team_z_offset
#define sPassageWayZoffset state->passage_offset
#define sDrawTeamName state->team_name_visible
#define Title_SetLightRot(...) ((void)0)
#define Audio_KillSfxById(...) ((void)0)
#define gDrawMode state->draw_mode
#define DRAW_NONE 0
#define AUDIO_PLAY_SFX(...) ((void)0)
#define Audio_KillSfxBySourceAndId(...) ((void)0)
#define Radio_PlayMessage(...) ((void)0)
#define Title_Radio_PlayMessage(...) ((void)0)
#define sStarfoxLogoAlpha state->starfox_alpha
#define sInAlpha state->in_alpha
#define D_menu_801B82CC state->fade_in_scale
#define D_menu_801B82D0 state->fade_out_scale
'''
    wrapper = '''
void sf64_opening_init(Sf64OpeningState *state) {
    memset(state, 0, sizeof(*state));
    state->fill_alpha = 255;
    state->fade_in_scale = 0.002f;
    state->n64_slide = -900.0f;
    state->logo_x = 110.0f;
    state->logo_y = 70.0f;
    state->logo_scale_x = state->logo_scale_y = 1.0f;
    state->star_speed = 1.0f;
    state->great_fox_position[0] = 20.0f;
    state->great_fox_position[2] = 30.0f;
    state->light_y = 162.0f;
    state->fade_mode = 255;
    state->eye[0] = opening_camera[0].eye.x;
    state->eye[1] = opening_camera[0].eye.y;
    state->eye[2] = opening_camera[0].eye.z;
    state->at[0] = opening_camera[0].at.x;
    state->at[1] = opening_camera[0].at.y;
    state->at[2] = opening_camera[0].at.z;
    state->up[1] = 1.0f;
    state->scene_state = 100;
}

void sf64_opening_update(Sf64OpeningState *state) {
    f32 temp, temp2;
    if (sTimer1 > 0) sTimer1--;
    if (sTimer2 > 0) sTimer2--;
    if (state->phase == 1) {
        opening_team_update(state);
        state->passage_offset += 70.0f;
        if (state->passage_offset >= 4101.6f) state->passage_offset -= 4101.6f;
        state->team_frames[0] += 5;
        state->team_frames[1] += 5;
        state->team_frames[2] += 6;
        state->team_frames[3] += 7;
        return;
    }
    if (state->phase == 2) {
        if (sSceneState == 0) {
            state->fill_alpha = 0;
            state->fade_mode = 255;
            state->camera_xrot = 10.0f;
            state->camera_yrot = -30.0f;
            state->camera_distance = 450.0f;
            state->light_x = state->light_y = 0.0f;
            state->at[0] = -10.0f;
            state->at[1] = state->at[2] = 0.0f;
            state->great_fox_position[0] = state->great_fox_position[1] = state->great_fox_position[2] = 0.0f;
            state->show_great_fox = 1;
            state->logo_state = -1;
            state->card_state = 0;
            state->show_sun = 0;
            state->fade_in_scale = 0.01f;
            sTimer3 = 0;
            sSceneState = 1;
        } else {
            opening_smooth_step(&state->camera_xrot, 30.0f, state->fade_in_scale, 100.0f, 0.0001f);
            opening_smooth_step(&state->camera_yrot, -15.0f, state->fade_in_scale, 100.0f, 0.0001f);
            opening_smooth_step(&state->at[0], 0.0f, 0.05f, 100.0f, 0.0001f);
            opening_smooth_step(&state->at[1], -40.0f, state->fade_in_scale, 100.0f, 0.0001f);
            opening_smooth_step(&state->camera_distance, 100.0f, state->fade_in_scale, 100.0f, 0.0001f);
            state->fade_in_scale *= 1.04f;
            if (sTimer3 == 50) state->fade_mode = 1;
            if (state->fill_alpha == 255) {
                sSceneState = 0;
                state->phase = 3;
                state->finished = 1;
            }
            opening_fade(state);
            sTimer3++;
        }
        {
            float pitch = state->camera_xrot * 0.01745329252f;
            float yaw = state->camera_yrot * 0.01745329252f;
            state->eye[0] = state->at[0] + sinf(yaw) * cosf(pitch) * state->camera_distance;
            state->eye[1] = state->at[1] - sinf(pitch) * state->camera_distance;
            state->eye[2] = state->at[2] + cosf(yaw) * cosf(pitch) * state->camera_distance;
            opening_set_cam_up(state, state->camera_xrot, state->camera_yrot);
        }
        return;
    }
    switch (sSceneState) {
'''
    args.output.parent.mkdir(parents=True, exist_ok=True)
    helpers = '''
static void opening_get_cam_rot(Sf64OpeningState *state, f32 *xrot, f32 *yrot) {
    f32 x = gCsCamEyeX - gCsCamAtX, y = gCsCamEyeY - gCsCamAtY, z = gCsCamEyeZ - gCsCamAtZ;
    *xrot = -atan2f(y, sqrtf(x*x + z*z)) * 57.2957795131f;
    *yrot = atan2f(x, z) * 57.2957795131f;
}
static void opening_set_cam_up(Sf64OpeningState *state, f32 xrot, f32 yrot) {
    f32 x = xrot * 0.01745329252f, y = yrot * 0.01745329252f;
    state->up[0] = sinf(y) * sinf(x);
    state->up[1] = cosf(x);
    state->up[2] = cosf(y) * sinf(x);
}
static void opening_team_setup(Sf64OpeningState *state) {
    f32 pitch = 3.0f * 0.01745329252f, yaw = 7.0f * 0.01745329252f;
    f32 dx = sinf(yaw) * cosf(pitch), dy = -sinf(pitch), dz = cosf(yaw) * cosf(pitch);
    state->fill_alpha = 255;
    state->fill_red = state->fill_green = state->fill_blue = 0;
    state->fade_mode = 255;
    state->team_name_visible = 0;
    state->team_z_offset = -6000.0f;
    state->eye[0] = 22.0f + dx * 400.0f;
    state->eye[1] = 116.0f + dy * 400.0f;
    state->eye[2] = -543.0f + dz * 400.0f;
    state->at[0] = 22.0f - dx * 100.0f;
    state->at[1] = 116.0f - dy * 100.0f;
    state->at[2] = -543.0f - dz * 100.0f;
    opening_set_cam_up(state, 3.0f, 7.0f);
    state->team_frames[0] = 14;
    state->team_frames[1] = 0;
    state->team_frames[2] = 29;
    state->team_frames[3] = 41;
}
'''
    args.output.write_text(header + camera_data + team_data + smooth + camera_calc + camera_step + fade + helpers + team_update + wrapper + cases +
                           "        default: break;\n    }\n    opening_fade(state);\n    sTitleMsgFrameCount++;\n}\n")


if __name__ == "__main__":
    main()

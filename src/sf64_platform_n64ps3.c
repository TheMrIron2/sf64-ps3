#include "sf64_ps3/platform.h"
#include "sf64_ps3/port_config.h"
#include "sf64_ps3/arwing.h"
#include "sf64_ps3/title_ui.h"
#include "sf64_ps3/title_starfield.h"
#include "sf64_ps3/title_runtime.h"
#include "sf64_ps3/controller.h"
#include "sf64_ps3/boot.h"
#include "sf64_ps3/takeoff.h"

#include "n64ps3/n64ps3.h"
#include "n64ps3/gfx_f3dex1.h"

#include <stdio.h>
#include <string.h>

#if defined(SF64_PS3_TARGET)
#include <io/pad.h>
#if defined(SF64_PS3_BOOT_PROBE)
#include <lv2/systime.h>
#endif
#endif

static OSMesgQueue s_vi_queue;
static OSMesg s_vi_messages[4];
static int s_queue_created;
static int s_vi_registered;
static int s_controller_initialized;
#if defined(SF64_PS3_ARWING_PROBE)
static int s_original_title_logic_initialized;
#endif

#if defined(SF64_PS3_TARGET)
static int s_controller_owned;
static Sf64ControllerSample s_controller_sample = { 0, 0, 128, 128 };
static Sf64ControllerState s_controller_state;

static void sf64_platform_reset_controller(void)
{
    s_controller_sample.digital1 = 0;
    s_controller_sample.digital2 = 0;
    s_controller_sample.left_x = 128;
    s_controller_sample.left_y = 128;
    sf64_controller_update(&s_controller_state, NULL);
#if defined(SF64_PS3_ARWING_PROBE)
    if (s_original_title_logic_initialized) {
        sf64_original_title_logic_set_controller(0, 0, 0);
    }
#endif
}
#endif

#if defined(SF64_PS3_ARWING_PROBE)
extern const unsigned char sf64_arwing_segment_data[];
extern const unsigned int sf64_arwing_segment_size;
extern const unsigned char sf64_title_segment_data[];
extern const unsigned int sf64_title_segment_size;
/* Shared by the title-only harness and the combined boot-to-title build. */
static u8 s_arwing_rdram[SF64_ARWING_RDRAM_REQUIRED_SIZE];
static u8 s_title_starfield_commands[SF64_TITLE_STARFIELD_RDRAM_SIZE];
static u32 s_title_starfield_command_bytes;
static u32 s_title_star_count;
static int s_arwing_task_logged;
#else

static const u8 s_f3dex1_root_display_list[] = {
    0xbc, 0x00, 0x18, 0x06, 0x80, 0x00, 0x01, 0x00,
    0x01, 0x06, 0x00, 0x40, 0x06, 0x00, 0x00, 0x00,
    0x01, 0x02, 0x00, 0x40, 0x06, 0x00, 0x00, 0x40,
    0xb7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0xc0,
    0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const u8 s_f3dex1_nested_display_list[] = {
    0x04, 0x00, 0x0c, 0x2f, 0x06, 0x00, 0x00, 0x80,
    0xbf, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x04,
    0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const u8 s_f3dex1_vertices[] = {
    0xa2, 0x40, 0xb5, 0xc8, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xff, 0xd1, 0x1f, 0xff,
    0x5d, 0xc0, 0xb5, 0xc8, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xff, 0xd1, 0x1f, 0xff,
    0x00, 0x00, 0x5d, 0xc0, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xff, 0xd1, 0x1f, 0xff
};

static u8 s_f3dex1_dram[0x200];

static void sf64_platform_write_be16(u8 *destination, u16 value)
{
    destination[0] = (u8)(value >> 8);
    destination[1] = (u8)value;
}

static void sf64_platform_write_matrix_fixed(u8 *matrix,
                                             u32 row,
                                             u32 column,
                                             s16 integer,
                                             u16 fraction)
{
    u32 index = row * 4U + column;

    sf64_platform_write_be16(matrix + index * 2U, (u16)integer);
    sf64_platform_write_be16(matrix + 32U + index * 2U, fraction);
}

static void sf64_platform_make_identity_matrix(u8 *matrix)
{
    u32 axis;

    memset(matrix, 0, 64U);
    for (axis = 0; axis < 4U; axis++) {
        sf64_platform_write_matrix_fixed(matrix, axis, axis, 1, 0);
    }
}

static int sf64_platform_count_triangle(void *user_data,
                                       const N64ps3GfxF3dex1Vertex *a,
                                       const N64ps3GfxF3dex1Vertex *b,
                                       const N64ps3GfxF3dex1Vertex *c)
{
    u32 *count = (u32 *)user_data;

    if (a->x != -24000 || a->y != -19000 ||
        b->x != 24000 || b->y != -19000 ||
        c->x != 0 || c->y != 24000 ||
        a->geometry_mode != 0U ||
        b->geometry_mode != a->geometry_mode ||
        c->geometry_mode != a->geometry_mode ||
        a->clip_x < -0.7013f || a->clip_x > -0.7010f ||
        a->clip_y < -0.5800f || a->clip_y > -0.5796f ||
        b->clip_x < 0.7634f || b->clip_x > 0.7639f ||
        c->clip_y < 0.7321f || c->clip_y > 0.7328f ||
        a->color[0] != 255 || a->color[1] != 209 ||
        a->color[2] != 31 || a->color[3] != 255) {
        printf("[sf64-ps3] decoded vertex mismatch: raw=(%d,%d),(%d,%d),(%d,%d) clip=(%.6f,%.6f),(%.6f,%.6f),(%.6f,%.6f)\n",
               a->x, a->y, b->x, b->y, c->x, c->y,
               a->clip_x, a->clip_y, b->clip_x, b->clip_y,
               c->clip_x, c->clip_y);
        return -1;
    }
    (*count)++;
    return 0;
}
#endif

#if defined(SF64_PS3_ARWING_PROBE)
/* Render one original title-screen frame: starfield, Arwing, team, logos. */
static int sf64_platform_submit_title_frame(void)
{
    N64ps3GfxTask task;
    Sf64ArwingBuildInfo info;
    Sf64TitleUiInfo title_ui_info;
    Sf64OriginalTitleLighting team_lighting;
    Sf64OriginalTitleArwingPose original_pose;
    Sf64ArwingTitlePose arwing_pose;
    Sf64TitleTeamModel team_models[4];
    u32 team_limb_count = 0U;
    u32 team_display_list_count = 0U;
    float n64_logo_position[3];
    int title_draw_active;

    if (!s_original_title_logic_initialized) {
        s32 game_state;
        s32 title_state;
        s32 cutscene_state;
        s32 main_controller;

        if (sf64_title_starfield_init() != 0 ||
            sf64_original_title_logic_init() != 0) {
            printf("[sf64-ps3] original title state initialization failed\n");
            return -1;
        }
        sf64_original_title_logic_get_state(&game_state, &title_state,
                                            &cutscene_state, &main_controller);
        printf("[sf64-ps3] original title boot: game=%d title=%d cutscene=%d pad=%d\n",
               game_state, title_state, cutscene_state, main_controller);
        s_original_title_logic_initialized = 1;
    }
    if (sf64_original_title_logic_update() != 0 ||
        sf64_original_title_logic_get_arwing_pose(&original_pose) != 0 ||
        sf64_original_title_logic_get_64_logo_position(n64_logo_position) != 0 ||
        sf64_original_title_logic_get_team_lighting(&team_lighting) != 0) {
        printf("[sf64-ps3] original Title_Screen_Update failed\n");
        return -1;
    }
#if defined(SF64_PS3_STATIC_ARWING_POSE)
    original_pose.rotation_degrees[0] = 35.0f;
#endif

    memcpy(arwing_pose.position, original_pose.position,
           sizeof(arwing_pose.position));
    arwing_pose.scale = original_pose.scale;
    memcpy(arwing_pose.rotation_degrees, original_pose.rotation_degrees,
           sizeof(arwing_pose.rotation_degrees));
    sf64_arwing_set_title_pose(&arwing_pose);

    if (sf64_title_starfield_build_current(
            s_title_starfield_commands,
            (u32)sizeof(s_title_starfield_commands),
            &s_title_starfield_command_bytes,
            &s_title_star_count) != 0) {
        printf("[sf64-ps3] original title starfield update failed\n");
        return -1;
    }
    sf64_title_ui_reset();
    title_draw_active = sf64_original_title_logic_draw();
    if (title_draw_active <= 0) {
        if (title_draw_active == 0) {
            printf("[sf64-ps3] original title draw gate is inactive\n");
        } else {
            printf("[sf64-ps3] original title draw dispatch failed\n");
        }
        return -1;
    }
    if (sf64_arwing_build_title_task_with_starfield(
            sf64_arwing_segment_data,
            sf64_arwing_segment_size,
            s_title_starfield_commands,
            s_title_starfield_command_bytes,
            s_title_star_count,
            s_arwing_rdram,
            (u32)sizeof(s_arwing_rdram),
            &task,
            &info) != 0) {
        printf("[sf64-ps3] failed to build original title starfield/Arwing task\n");
        return -1;
    }
    {
        u32 team_index;
        memset(team_models, 0, sizeof(team_models));
        for (team_index = 0U; team_index < 4U; team_index++) {
            if (sf64_original_title_logic_get_team_model(
                    (int)team_index,
                    &team_models[team_index].skeleton_address,
                    &team_models[team_index].limb_count,
                    team_models[team_index].position,
                    team_models[team_index].rotation_degrees,
                    &team_models[team_index].scale,
                    team_models[team_index].joint_rotation_degrees) != 0) {
                printf("[sf64-ps3] failed to read original title team pose %u\n",
                       team_index);
                return -1;
            }
        }
    }
    if (sf64_arwing_append_title_teams(&task,
                                       sf64_title_segment_data,
                                       sf64_title_segment_size,
                                       team_models,
                                       &team_lighting,
                                       &team_limb_count,
                                       &team_display_list_count) != 0) {
        printf("[sf64-ps3] failed to append original title team skeletons\n");
        return -1;
    }
    if (sf64_title_ui_append_logo(&task,
                                  sf64_title_segment_data,
                                  sf64_title_segment_size,
                                  n64_logo_position,
                                  &title_ui_info) != 0) {
        printf("[sf64-ps3] failed to append original Star Fox title logo: %s\n",
               sf64_title_ui_last_error());
        return -1;
    }
    if (!s_arwing_task_logged) {
        printf("[sf64-ps3] original Title_Screen_Update pose "
               "pos=(%.1f,%.1f,%.1f) rot=(%.1f,%.1f,%.1f) scale=%.1f\n",
               original_pose.position[0], original_pose.position[1],
               original_pose.position[2], original_pose.rotation_degrees[0],
               original_pose.rotation_degrees[1], original_pose.rotation_degrees[2],
               original_pose.scale);
        printf("[sf64-ps3] title frame starfield stars=%u commands=%u; "
               "Arwing skeleton limbs=%u display-lists=%u "
               "wing-frame=%u root-commands=%u\n",
               s_title_star_count,
               s_title_starfield_command_bytes / 8U,
               info.skeleton_limb_count, info.display_list_count,
               info.animation_frame, info.generated_command_count);
        printf("[sf64-ps3] original Title_Draw logo strips=%u generated-commands=%u\n",
               title_ui_info.logo_strip_count,
               title_ui_info.generated_command_count);
        printf("[sf64-ps3] original 64 logo panels=%u triangles=%u opaque-texels=%u "
               "position=(%.1f,%.1f,%.1f) generated-display-list=1\n",
               title_ui_info.n64_logo_panel_count,
               title_ui_info.n64_logo_triangle_count,
               title_ui_info.n64_logo_opaque_texels,
               n64_logo_position[0], n64_logo_position[1],
               n64_logo_position[2]);
        printf("[sf64-ps3] original title team skeletons limbs=%u display-lists=%u "
               "fox=(%.1f,%.1f,%.1f) falco=(%.1f,%.1f,%.1f); "
               "press-start=%u\n",
               team_limb_count, team_display_list_count,
               team_models[0].position[0], team_models[0].position[1],
               team_models[0].position[2], team_models[1].position[0],
               team_models[1].position[1], team_models[1].position[2],
               title_ui_info.press_start_drawn);
        printf("[sf64-ps3] original title team light rgb=(%u,%u,%u) "
               "ambient=(%u,%u,%u) direction=(%.1f,%.1f,%.1f)\n",
               team_lighting.directional_rgb[0],
               team_lighting.directional_rgb[1],
               team_lighting.directional_rgb[2],
               team_lighting.ambient_rgb[0], team_lighting.ambient_rgb[1],
               team_lighting.ambient_rgb[2], team_lighting.direction[0],
               team_lighting.direction[1], team_lighting.direction[2]);
        s_arwing_task_logged = 1;
    }

    n64ps3_gfx_begin_frame();
    n64ps3_gfx_submit_task(&task);
    n64ps3_gfx_end_frame();
    return 0;
}
#endif

void sf64_platform_print_banner(void)
{
#if defined(SF64_PS3_ARWING_PROBE)
    printf("[sf64-ps3] original Star Fox 64 Arwing title-pose probe\n");
#else
    printf("[sf64-ps3] smoke target: %s\n", SF64_PS3_BUILD_FLAVOR);
    printf("[sf64-ps3] probing n64ps3/libultra-shaped APIs\n");
#endif
}

Sf64ApiState sf64_platform_init(void)
{
    N64ps3Config config = n64ps3_config_default();

#if defined(SF64_PS3_ARWING_PROBE)
    config.app_name = "sf64-ps3-arwing";
#else
    config.app_name = "sf64-ps3-smoke";
#endif
    config.asset_root = "assets";
    config.save_root = "saves";
    config.vi_refresh_hz = 60;

    return n64ps3_runtime_init(&config) == 0 ? SF64_API_WORKED : SF64_API_FAILED;
}

Sf64ApiState sf64_platform_create_vi_queue(void)
{
    memset(&s_vi_queue, 0, sizeof(s_vi_queue));
    memset(s_vi_messages, 0, sizeof(s_vi_messages));

    osCreateMesgQueue(&s_vi_queue, s_vi_messages, 4);
    s_queue_created = 1;
    return SF64_API_WORKED;
}

Sf64ApiState sf64_platform_register_vi_event(void)
{
    if (!s_queue_created) {
        return SF64_API_FAILED;
    }

    osViSetEvent(&s_vi_queue, SF64_PS3_VI_MESSAGE_VALUE, 1);
    s_vi_registered = 1;
    return SF64_API_WORKED;
}

Sf64ApiState sf64_platform_trigger_vi_retrace(void)
{
    if (!s_queue_created || !s_vi_registered) {
        return SF64_API_FAILED;
    }

    n64ps3_vi_raise_retrace_for_host();
    return SF64_API_WORKED;
}

Sf64ApiState sf64_platform_receive_vi_message(void)
{
    OSMesg msg = 0;
    OSMesg duplicate = 0;

    if (!s_queue_created) {
        return SF64_API_FAILED;
    }

    if (osRecvMesg(&s_vi_queue, &msg, OS_MESG_BLOCK) != 0) {
        return SF64_API_FAILED;
    }

    if (msg != SF64_PS3_VI_MESSAGE_VALUE) {
        printf("[sf64-ps3] VI message payload mismatch\n");
        return SF64_API_FAILED;
    }

    if (osRecvMesg(&s_vi_queue, &duplicate, OS_MESG_NOBLOCK) == 0) {
        printf("[sf64-ps3] duplicate VI message received for one retrace\n");
        return SF64_API_FAILED;
    }

    return SF64_API_WORKED;
}

Sf64ApiState sf64_platform_poll_controller(void)
{
#if defined(SF64_PS3_TARGET)
    padInfo2 info;
    padData data;

    if (!s_controller_initialized) {
        s32 result = ioPadInit(MAX_PORT_NUM);
        if (result != PAD_OK && (u32)result != PAD_ERROR_ALREADY_INITIALIZED) {
            sf64_platform_reset_controller();
            return SF64_API_FAILED;
        }
        s_controller_owned = result == PAD_OK;
        s_controller_initialized = 1;
    }
    memset(&info, 0, sizeof(info));
    if (ioPadGetInfo2(&info) != PAD_OK) {
        sf64_platform_reset_controller();
        return SF64_API_FAILED;
    }
    if ((info.port_status[0] & 1U) == 0U) {
        sf64_platform_reset_controller();
    } else {
        memset(&data, 0, sizeof(data));
        if (ioPadGetData(0, &data) != PAD_OK) {
            sf64_platform_reset_controller();
            return SF64_API_FAILED;
        }
        if (data.len > PAD_BUTTON_OFFSET_ANALOG_LEFT_Y) {
            s_controller_sample.digital1 = data.button[PAD_BUTTON_OFFSET_DIGITAL1];
            s_controller_sample.digital2 = data.button[PAD_BUTTON_OFFSET_DIGITAL2];
            s_controller_sample.left_x = (uint8_t)data.button[PAD_BUTTON_OFFSET_ANALOG_LEFT_X];
            s_controller_sample.left_y = (uint8_t)data.button[PAD_BUTTON_OFFSET_ANALOG_LEFT_Y];
        }
        sf64_controller_update(&s_controller_state, &s_controller_sample);
    }
#if defined(SF64_PS3_ARWING_PROBE)
    if (s_original_title_logic_initialized) {
        if (s_controller_state.pressed & 0x9000U) {
            printf("[sf64-ps3] option menu is not implemented yet\n");
        }
        sf64_original_title_logic_set_controller(
            s_controller_state.pressed & (uint16_t)~0x9000U,
            s_controller_state.stick_x, s_controller_state.stick_y);
    }
#endif
#else
    N64ps3ControllerState pad;

    memset(&pad, 0, sizeof(pad));
    if (!s_controller_initialized) {
        n64ps3_pad_init();
        s_controller_initialized = 1;
    }
    if (n64ps3_pad_read(0, &pad) != 0) {
        return SF64_API_FAILED;
    }
#if defined(SF64_PS3_ARWING_PROBE)
    if (s_original_title_logic_initialized) {
        sf64_original_title_logic_set_controller(pad.buttons,
                                                 pad.stick_x,
                                                 pad.stick_y);
    }
#endif
#endif
    return SF64_API_WORKED;
}

Sf64ApiState sf64_platform_probe_pi_read(void)
{
#if defined(SF64_PS3_ARWING_PROBE)
    if (!s_arwing_task_logged) {
        printf("[sf64-ps3] embedded US Rev 1 ast_arwing bank: %u bytes\n",
               sf64_arwing_segment_size);
    }
    return SF64_API_WORKED;
#else
    unsigned char byte = 0;

    if (n64ps3_pi_read_rom(0, &byte, (u32)sizeof(byte)) == 0) {
        return SF64_API_WORKED;
    }

    printf("[sf64-ps3] TODO(n64ps3): provide ROM-free PI/asset read smoke path\n");
    return SF64_API_MISSING;
#endif
}

Sf64ApiState sf64_platform_submit_f3dex1_gfx_task(void)
{
#if defined(SF64_PS3_BOOT_PROBE)
    extern const unsigned char sf64_boot_logo_data[];
    extern const unsigned int sf64_boot_logo_size;
    extern const unsigned char sf64_great_fox_segment_data[];
    extern const unsigned int sf64_great_fox_segment_size;
    static uint8_t boot_memory[SF64_BOOT_RDRAM_BYTES];
    static Sf64BootState boot;
    static Sf64OpeningState opening;
    static Sf64TakeoffState takeoff;
    static Sf64SpaceState space;
    static uint8_t opening_memory[SF64_OPENING_RDRAM_BYTES];
    static uint8_t team_memory[SF64_ARWING_RDRAM_REQUIRED_SIZE];
    static uint8_t opening_stars[SF64_TITLE_STARFIELD_DISPLAY_LIST_CAPACITY];
    static unsigned opening_stars_size;
    static unsigned opening_star_count;
    static uint64_t next_opening_tick;
    static int opening_initialized;
    static int takeoff_initialized;
    static int space_initialized;
    static int initialized;
    static int logged_done;
    static int title_entered;
    N64ps3GfxTask task;
    int visible;
    uint64_t now = (uint64_t)sysGetSystemTime();

    if (!initialized) {
        sf64_boot_init(&boot);
        initialized = 1;
    }
    visible = sf64_boot_update(&boot, now);
    if (boot.save_check_pending) {
        printf("[sf64-ps3] boot: save check not implemented\n");
        boot.save_check_pending = 0;
    }
    if (boot.state == 1 && !logged_done) {
        printf("[sf64-ps3] boot: reached GSTATE_INIT; starting opening logo sequence\n");
        logged_done = 1;
    }
    if (boot.state == 1) {
        if (!opening_initialized) {
            sf64_opening_init(&opening);
            next_opening_tick = now;
            opening_initialized = 1;
        }
        while (now >= next_opening_tick && !opening.finished) {
            sf64_opening_update(&opening);
            if ((opening.phase == 0 || opening.phase == 2) && sf64_title_starfield_build_opening(opening_stars, sizeof(opening_stars),
                                                    &opening_stars_size,
                                                    &opening_star_count,
                                                    opening.eye, opening.at, opening.star_speed) != 0) {
                return SF64_API_FAILED;
            }
            next_opening_tick += 33333U;
            if (now >= next_opening_tick && now - next_opening_tick > 133332U) {
                next_opening_tick = now + 33333U;
            }
        }
        if (opening.finished) {
#if defined(SF64_PS3_ARWING_PROBE)
            if (!takeoff_initialized) {
                printf("[sf64-ps3] opening complete; starting takeoff scene\n");
                sf64_takeoff_init(&takeoff);
                takeoff_initialized = 1;
            }
            if (!takeoff.finished) {
                sf64_takeoff_update(&takeoff);
                if (sf64_takeoff_build_task(
                        &task, team_memory, sizeof(team_memory),
                        sf64_arwing_segment_data, sf64_arwing_segment_size,
                        sf64_title_segment_data, sf64_title_segment_size,
                        &takeoff) != 0) {
                    printf("[sf64-ps3] failed to build takeoff task\n");
                    return SF64_API_FAILED;
                }
                n64ps3_gfx_begin_frame();
                n64ps3_gfx_submit_task(&task);
                n64ps3_gfx_end_frame();
                return SF64_API_WORKED;
            }
            if (!space_initialized) {
                printf("[sf64-ps3] takeoff complete; starting space fly-by\n");
                sf64_space_init(&space);
                space_initialized = 1;
            }
            if (!space.finished) {
                sf64_space_update(&space);
                if (sf64_space_build_task(
                        &task, team_memory, sizeof(team_memory),
                        sf64_arwing_segment_data, sf64_arwing_segment_size,
                        sf64_title_segment_data, sf64_title_segment_size,
                        sf64_great_fox_segment_data, sf64_great_fox_segment_size,
                        &space) != 0) {
                    printf("[sf64-ps3] failed to build space task\n");
                    return SF64_API_FAILED;
                }
                n64ps3_gfx_begin_frame();
                n64ps3_gfx_submit_task(&task);
                n64ps3_gfx_end_frame();
                return SF64_API_WORKED;
            }
            if (!title_entered) {
                printf("[sf64-ps3] space complete; entering original title screen\n");
                title_entered = 1;
            }
            return sf64_platform_submit_title_frame() == 0
                       ? SF64_API_WORKED
                       : SF64_API_FAILED;
#else
            if (sf64_boot_build_task(&task, boot_memory, sizeof(boot_memory),
                                     sf64_boot_logo_data, sf64_boot_logo_size, 0) != 0) {
                return SF64_API_FAILED;
            }
            n64ps3_gfx_begin_frame();
            n64ps3_gfx_submit_task(&task);
            n64ps3_gfx_end_frame();
            return SF64_API_WORKED;
#endif
        }
        if (opening.phase == 1 && opening.scene_state != 0) {
            float fade = 1.0f - (float)opening.fill_alpha / 255.0f;
            if (sf64_team_running_build(&task, team_memory, sizeof(team_memory),
                                        sf64_title_segment_data, sf64_title_segment_size,
                                        opening.eye, opening.at, opening.up,
                                        opening.team_z_offset, opening.passage_offset,
                                        fade, opening.team_light_dir,
                                        opening.team_ambient, opening.team_frames,
                                        opening.team_name_visible,
                                        opening.team_name_index) != 0) {
                printf("[sf64-ps3] opening: failed to build team-running task\n");
                return SF64_API_FAILED;
            }
            n64ps3_gfx_begin_frame();
            n64ps3_gfx_submit_task(&task);
            n64ps3_gfx_end_frame();
            return SF64_API_WORKED;
        }
        if (opening.phase != 0 && opening.phase != 2) {
            if (sf64_boot_build_task(&task, boot_memory, sizeof(boot_memory),
                                     sf64_boot_logo_data, sf64_boot_logo_size, 0) != 0) {
                return SF64_API_FAILED;
            }
            n64ps3_gfx_begin_frame();
            n64ps3_gfx_submit_task(&task);
            n64ps3_gfx_end_frame();
            return SF64_API_WORKED;
        }
        if (sf64_opening_build_task(&task, opening_memory, sizeof(opening_memory),
                                    sf64_title_segment_data, sf64_title_segment_size,
                                    opening_stars, opening_stars_size, &opening,
                                    sf64_great_fox_segment_data, sf64_great_fox_segment_size) != 0) {
            return SF64_API_FAILED;
        }
        n64ps3_gfx_begin_frame();
        n64ps3_gfx_submit_task(&task);
        n64ps3_gfx_end_frame();
        return SF64_API_WORKED;
    }
    if (sf64_boot_build_task(&task, boot_memory, sizeof(boot_memory),
                             sf64_boot_logo_data, sf64_boot_logo_size, visible) != 0) {
        return SF64_API_FAILED;
    }
    n64ps3_gfx_begin_frame();
    n64ps3_gfx_submit_task(&task);
    n64ps3_gfx_end_frame();
    return SF64_API_WORKED;
#elif defined(SF64_PS3_ARWING_PROBE)
    return sf64_platform_submit_title_frame() == 0
               ? SF64_API_WORKED
               : SF64_API_FAILED;
#else
    N64ps3GfxTask task;
    N64ps3GfxF3dex1Stats stats;
    N64ps3GfxF3dex1Status decode_status;
    u32 triangle_callbacks = 0;

    memset(s_f3dex1_dram, 0, sizeof(s_f3dex1_dram));
    sf64_platform_write_matrix_fixed(s_f3dex1_dram + 0x100, 0, 0, 0, 2);
    sf64_platform_write_matrix_fixed(s_f3dex1_dram + 0x100, 1, 1, 0, 2);
    sf64_platform_write_matrix_fixed(s_f3dex1_dram + 0x100, 2, 2, 0, 2);
    sf64_platform_write_matrix_fixed(s_f3dex1_dram + 0x100, 3, 3, 1, 0);
    sf64_platform_make_identity_matrix(s_f3dex1_dram + 0x140);
    sf64_platform_write_matrix_fixed(s_f3dex1_dram + 0x140, 3, 0, 1024, 0);
    memcpy(s_f3dex1_dram + 0x180, s_f3dex1_vertices, sizeof(s_f3dex1_vertices));
    memcpy(s_f3dex1_dram + 0x1c0, s_f3dex1_nested_display_list,
           sizeof(s_f3dex1_nested_display_list));

    memset(&task, 0, sizeof(task));
    task.kind = N64PS3_GFX_TASK_DISPLAY_LIST;
    task.dram_base = s_f3dex1_dram;
    task.dram_size_bytes = (u32)sizeof(s_f3dex1_dram);
    task.display_list = s_f3dex1_root_display_list;
    task.display_list_size_bytes = (u32)sizeof(s_f3dex1_root_display_list);
    task.debug_name = "SF64-shaped F3DEX1 triangle display list";

    decode_status = n64ps3_gfx_decode_f3dex1(&task, sf64_platform_count_triangle,
                                             &triangle_callbacks, &stats);
    if (decode_status != N64PS3_GFX_F3DEX1_OK || stats.triangle_count != 1 ||
        triangle_callbacks != 1 || stats.display_list_call_count != 1) {
        printf("[sf64-ps3] F3DEX1 decode failed: status=%d commands=%u triangles=%u\n",
               (int)decode_status, stats.command_count, stats.triangle_count);
        return SF64_API_FAILED;
    }
    printf("[sf64-ps3] F3DEX1 commands=%u loads=%u triangles=%u nested=%u\n",
           stats.command_count, stats.vertex_load_count, stats.triangle_count,
           stats.display_list_call_count);

    n64ps3_gfx_begin_frame();
    n64ps3_gfx_submit_task(&task);
    n64ps3_gfx_end_frame();
    return SF64_API_WORKED;
#endif
}

void sf64_platform_shutdown(void)
{
#if defined(SF64_PS3_TARGET)
    if (s_controller_owned) {
        ioPadEnd();
        s_controller_owned = 0;
    }
    s_controller_initialized = 0;
#endif
    n64ps3_runtime_shutdown();
}

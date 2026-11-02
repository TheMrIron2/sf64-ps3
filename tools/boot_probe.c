#include "sf64_ps3/boot.h"
#include "n64ps3/gfx_f3dex1.h"
#include "sf64_ps3/title_starfield.h"
#include "sf64_ps3/arwing.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

extern const unsigned char sf64_boot_logo_data[];
extern const unsigned int sf64_boot_logo_size;
extern const unsigned char sf64_title_segment_data[];
extern const unsigned int sf64_title_segment_size;
extern const unsigned char sf64_great_fox_segment_data[];
extern const unsigned int sf64_great_fox_segment_size;

static uint8_t s_memory[SF64_BOOT_RDRAM_BYTES];
static uint8_t s_opening_memory[SF64_OPENING_RDRAM_BYTES];
static uint8_t s_team_memory[SF64_ARWING_RDRAM_REQUIRED_SIZE];
static uint8_t s_starfield[SF64_TITLE_STARFIELD_DISPLAY_LIST_CAPACITY];
static const uint8_t s_empty_stars[8] = {0xb8, 0, 0, 0, 0, 0, 0, 0};
static const uint8_t s_test_star[24] = {
    0xf7, 0, 0, 0, 0xff, 0xff, 0xff, 0xff,
    0xf6, 0x02, 0xc0, 0x2c, 0x00, 0x02, 0x80, 0x28,
    0xb8, 0, 0, 0, 0, 0, 0, 0,
};

static int triangle(void *context, const N64ps3GfxF3dex1Vertex *a,
                    const N64ps3GfxF3dex1Vertex *b,
                    const N64ps3GfxF3dex1Vertex *c)
{
    unsigned *count = context;
    unsigned strip = *count / 2U;
    const uint8_t *expected = s_memory + 0x1000U + strip * 16U * 128U * 4U;
    uint8_t ia = sf64_boot_logo_data[strip * 16U * 128U];
    if (!a->texture_enabled || !a->texture_data || a->texture_width != 128 ||
        (a->texture_height != 16 && a->texture_height != 10) ||
        !a->blend_enabled || b->texture_data != a->texture_data ||
        c->texture_data != a->texture_data || a->texture_data != expected ||
        expected[0] != (ia >> 4) * 17U || expected[3] != (ia & 15U) * 17U) return -1;
    (*count)++;
    return 0;
}

static int opening_triangle(void *context, const N64ps3GfxF3dex1Vertex *a,
                            const N64ps3GfxF3dex1Vertex *b,
                            const N64ps3GfxF3dex1Vertex *c)
{
    unsigned *count = context;
    unsigned expected_width = *count < 2 ? 256U : 32U;
    if (!a->texture_data || a->texture_width != expected_width ||
        a->texture_height != 13 || !a->blend_enabled ||
        b->texture_data != a->texture_data || c->texture_data != a->texture_data) return -1;
    (*count)++;
    return 0;
}

static int n64_logo_triangle(void *context, const N64ps3GfxF3dex1Vertex *a,
                             const N64ps3GfxF3dex1Vertex *b,
                             const N64ps3GfxF3dex1Vertex *c)
{
    unsigned *count = context;
    if (!a->texture_data || a->texture_width != 128 ||
        a->texture_height != (*count < 10 ? 16 : 8) || !a->blend_enabled ||
        b->texture_data != a->texture_data || c->texture_data != a->texture_data) return -1;
    (*count)++;
    return 0;
}

static int count_all(void *context, const N64ps3GfxF3dex1Vertex *a,
                     const N64ps3GfxF3dex1Vertex *b,
                     const N64ps3GfxF3dex1Vertex *c)
{
    unsigned *count = context;
    (void)a;
    (void)b;
    (void)c;
    (*count)++;
    return 0;
}

typedef struct GreatFoxProbe {
    unsigned triangles;
    unsigned visible_vertices;
} GreatFoxProbe;

static int check_great_fox(void *context, const N64ps3GfxF3dex1Vertex *a,
                           const N64ps3GfxF3dex1Vertex *b,
                           const N64ps3GfxF3dex1Vertex *c)
{
    GreatFoxProbe *probe = context;
    const N64ps3GfxF3dex1Vertex *verts[3] = {a, b, c};
    unsigned i;
    if (!a->texture_data || a->texture_data < s_opening_memory + 0x18000U ||
        a->texture_data >= s_opening_memory + 0x18000U + sf64_great_fox_segment_size) return 0;
    probe->triangles++;
    for (i = 0; i < 3; i++) {
        if (verts[i]->clip_valid && verts[i]->clip_w > 0.0f &&
            verts[i]->clip_x >= -verts[i]->clip_w && verts[i]->clip_x <= verts[i]->clip_w &&
            verts[i]->clip_y >= -verts[i]->clip_w && verts[i]->clip_y <= verts[i]->clip_w &&
            verts[i]->clip_z >= -verts[i]->clip_w && verts[i]->clip_z <= verts[i]->clip_w) {
            probe->visible_vertices++;
        }
    }
    return 0;
}

int main(void)
{
    Sf64BootState boot;
    Sf64OpeningState opening;
    N64ps3GfxTask task;
    N64ps3GfxF3dex1Stats stats;
    unsigned count = 0;
    unsigned frame;
    GreatFoxProbe great_fox = {0};
    unsigned star_count, stars_size;
    int saw_starfox = 0, saw_in = 0;
    unsigned team;

    sf64_boot_init(&boot);
    assert(sf64_boot_update(&boot, 0) == 0 && boot.state == 101);
    assert(sf64_boot_update(&boot, 16667) == 0 && boot.state == 101);
    assert(sf64_boot_update(&boot, 33334) == 0 && boot.state == 102);
    assert(sf64_boot_update(&boot, 50001) == 1 && boot.state == 103);
    assert(sf64_boot_update(&boot, 66668) == 1 && boot.state == 104);
    assert(boot.save_check_pending && boot.logo_deadline_us == 1066668);
    assert(sf64_boot_update(&boot, 1066667) == 1 && boot.state == 104);
    assert(sf64_boot_update(&boot, 1066668) == 0 && boot.state == 1);

    assert(sf64_boot_build_task(&task, s_memory, sizeof(s_memory),
                                sf64_boot_logo_data, sf64_boot_logo_size, 1) == 0);
    assert(n64ps3_gfx_decode_f3dex1(&task, triangle, &count, &stats) == N64PS3_GFX_F3DEX1_OK);
    assert(count == 10 && stats.texture_load_count == 5 && stats.unsupported_command_count == 0);
    printf("Nintendo boot logo: %u triangles, %u texture loads, %u skipped commands\n",
           count, stats.texture_load_count, stats.unsupported_command_count);

    sf64_opening_init(&opening);
    assert(sf64_opening_build_task(&task, s_opening_memory, sizeof(s_opening_memory),
                                    sf64_title_segment_data, sf64_title_segment_size,
                                    s_test_star, sizeof(s_test_star), &opening,
                                    sf64_great_fox_segment_data, sf64_great_fox_segment_size) == 0);
    assert(s_opening_memory[4] == 0 && s_opening_memory[5] == 1 &&
           s_opening_memory[6] == 0 && s_opening_memory[7] == 1);
    for (frame = 0; frame < 1000 && opening.scene_state != 3; frame++) {
        sf64_opening_update(&opening);
        if (opening.starfox_alpha > 200.0f) saw_starfox = 1;
        if (opening.in_alpha > 200.0f) saw_in = 1;
    }
    assert(saw_starfox && saw_in && opening.scene_state == 3);
    count = 0;
    assert(sf64_opening_build_task(&task, s_opening_memory, sizeof(s_opening_memory),
                                    sf64_title_segment_data, sf64_title_segment_size,
                                    s_empty_stars, sizeof(s_empty_stars), &opening,
                                    sf64_great_fox_segment_data, sf64_great_fox_segment_size) == 0);
    assert(n64ps3_gfx_decode_f3dex1(&task, opening_triangle, &count, &stats) == N64PS3_GFX_F3DEX1_OK);
    assert(count == 4 && stats.texture_load_count == 2 && stats.unsupported_command_count == 0);
    printf("opening logo states: %u updates; %u triangles, %u textures, %u skipped commands\n",
           frame, count, stats.texture_load_count, stats.unsupported_command_count);
    for (; frame < 2000 && opening.scene_state != 10; frame++) {
        sf64_opening_update(&opening);
    }
    assert(opening.scene_state == 10 && opening.logo_state == 1 && opening.starfox_alpha == 255.0f);
    count = 0;
    assert(sf64_opening_build_task(&task, s_opening_memory, sizeof(s_opening_memory),
                                    sf64_title_segment_data, sf64_title_segment_size,
                                    s_empty_stars, sizeof(s_empty_stars), &opening,
                                    sf64_great_fox_segment_data, sf64_great_fox_segment_size) == 0);
    assert(n64ps3_gfx_decode_f3dex1(&task, n64_logo_triangle, &count, &stats) == N64PS3_GFX_F3DEX1_OK);
    assert(count == 12 && stats.texture_load_count == 6 && stats.unsupported_command_count == 0);
    printf("opening Nintendo 64 logo: %u updates; %u triangles, %u textures\n",
           frame, count, stats.texture_load_count);
    assert(sf64_title_starfield_build_opening(s_starfield, sizeof(s_starfield),
                                               &stars_size, &star_count,
                                               opening.eye, opening.at, opening.star_speed) == 0);
    assert(star_count > 0);
    assert(sf64_opening_build_task(&task, s_opening_memory, sizeof(s_opening_memory),
                                    sf64_title_segment_data, sf64_title_segment_size,
                                    s_starfield, stars_size, &opening,
                                    sf64_great_fox_segment_data, sf64_great_fox_segment_size) == 0);
    count = 0;
    assert(n64ps3_gfx_decode_f3dex1(&task, count_all, &count, &stats) == N64PS3_GFX_F3DEX1_OK);
    assert(stats.fill_rectangle_count == star_count && stats.triangle_count == star_count * 2U + 12U &&
           stats.texture_load_count == 6 && stats.unsupported_command_count == 0);
    printf("opening scene: %u stars, %u total triangles, %u skipped commands\n",
           star_count, count, stats.unsupported_command_count);
    for (; frame < 2000 && !opening.show_great_fox; frame++) {
        sf64_opening_update(&opening);
    }
    assert(opening.show_great_fox && opening.logo_state == 1);
    assert(sf64_opening_build_task(&task, s_opening_memory, sizeof(s_opening_memory),
                                    sf64_title_segment_data, sf64_title_segment_size,
                                    s_starfield, stars_size, &opening,
                                    sf64_great_fox_segment_data, sf64_great_fox_segment_size) == 0);
    assert(n64ps3_gfx_decode_f3dex1(&task, check_great_fox, &great_fox, &stats) == N64PS3_GFX_F3DEX1_OK);
    assert(great_fox.triangles == 316 && great_fox.visible_vertices > 0);
    great_fox.triangles = great_fox.visible_vertices = 0;
    for (; frame < 2000 && opening.timer3 < 120; frame++) sf64_opening_update(&opening);
    assert(opening.show_great_fox && opening.timer3 >= 120 && opening.camera_weight > 0.0f);
    assert(sf64_opening_build_task(&task, s_opening_memory, sizeof(s_opening_memory),
                                    sf64_title_segment_data, sf64_title_segment_size,
                                    s_starfield, stars_size, &opening,
                                    sf64_great_fox_segment_data, sf64_great_fox_segment_size) == 0);
    count = 0;
    assert(n64ps3_gfx_decode_f3dex1(&task, count_all, &count, &stats) == N64PS3_GFX_F3DEX1_OK);
    assert(stats.triangle_count >= star_count * 2U + 316U && stats.unsupported_command_count == 0);
    assert(n64ps3_gfx_decode_f3dex1(&task, check_great_fox, &great_fox, &stats) == N64PS3_GFX_F3DEX1_OK);
    assert(great_fox.triangles == 316 && great_fox.visible_vertices > 0);
    printf("Great Fox scene: %u triangles, %u textures, %u skipped commands\n",
           count, stats.texture_load_count, stats.unsupported_command_count);
    printf("Great Fox: %u model triangles, %u vertices inside the view\n",
           great_fox.triangles, great_fox.visible_vertices);
    for (; frame < 3000 && opening.phase == 0; frame++) sf64_opening_update(&opening);
    assert(opening.phase == 1 && opening.corneria.draw && opening.fill_alpha == 255);
    printf("Great Fox camera and fade complete after %u updates\n", frame);
    sf64_opening_update(&opening);
    for (; frame < 3000 && opening.team_z_offset != 0.0f; frame++) sf64_opening_update(&opening);
    assert(opening.phase == 1 && opening.team_z_offset == 0.0f);
    assert(sf64_team_running_build(&task, s_team_memory, sizeof(s_team_memory),
                                    sf64_title_segment_data, sf64_title_segment_size,
                                    opening.eye, opening.at, opening.up, opening.team_z_offset,
                                    opening.passage_offset, 1.0f, opening.team_frames,
                                    opening.team_name_visible, opening.team_name_index) == 0);
    count = 0;
    {
        N64ps3GfxF3dex1Status status = n64ps3_gfx_decode_f3dex1(&task, count_all, &count, &stats);
        printf("team running: status=%d triangles=%u textures=%u skipped=%u\n",
               status, stats.triangle_count, stats.texture_load_count, stats.unsupported_command_count);
        assert(status == N64PS3_GFX_F3DEX1_OK && stats.triangle_count > 0);
    }
    for (; frame < 4000 && opening.phase == 1; frame++) sf64_opening_update(&opening);
    assert(opening.phase == 2 && !opening.finished);
    printf("team-running scene completes after %u updates\n", frame);
    sf64_opening_update(&opening);
    frame++;
    great_fox.triangles = great_fox.visible_vertices = 0;
    assert(sf64_opening_build_task(&task, s_opening_memory, sizeof(s_opening_memory),
                                    sf64_title_segment_data, sf64_title_segment_size,
                                    s_starfield, stars_size, &opening,
                                    sf64_great_fox_segment_data, sf64_great_fox_segment_size) == 0);
    assert(n64ps3_gfx_decode_f3dex1(&task, check_great_fox, &great_fox, &stats) == N64PS3_GFX_F3DEX1_OK);
    assert(great_fox.triangles == 316 && great_fox.visible_vertices > 0);
    printf("Great Fox close-up: %u model triangles, %u vertices in view\n",
           great_fox.triangles, great_fox.visible_vertices);
    for (; frame < 4000 && !opening.finished; frame++) sf64_opening_update(&opening);
    assert(opening.finished && opening.phase == 3);
    printf("Great Fox close-up completes after %u updates\n", frame);
    for (team = 0; team < 4U; team++) {
        Sf64TitleTeamModel first = {0}, next = {0}, wrapped = {0};
        float first_root[3], next_root[3], wrapped_root[3];
        assert(sf64_title_running_animation(sf64_title_segment_data, sf64_title_segment_size,
                                             team, 0, &first, first_root) == 0);
        assert(sf64_title_running_animation(sf64_title_segment_data, sf64_title_segment_size,
                                             team, 1, &next, next_root) == 0);
        assert(sf64_title_running_animation(sf64_title_segment_data, sf64_title_segment_size,
                                             team, 60, &wrapped, wrapped_root) == 0);
        assert(first.limb_count == (team == 3U ? 20U : 22U));
        assert(memcmp(first.joint_rotation_degrees, next.joint_rotation_degrees,
                      sizeof(first.joint_rotation_degrees)) != 0 ||
               memcmp(first_root, next_root, sizeof(first_root)) != 0);
        assert(memcmp(first.joint_rotation_degrees, wrapped.joint_rotation_degrees,
                      sizeof(first.joint_rotation_degrees)) == 0 &&
               memcmp(first_root, wrapped_root, sizeof(first_root)) == 0);
    }
    assert(sf64_title_running_animation(sf64_title_segment_data, sf64_title_segment_size,
                                         4, 0, NULL, NULL) != 0);
    puts("running animations: all four teams decode, advance, and wrap");
    return 0;
}

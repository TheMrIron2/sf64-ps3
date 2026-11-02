#include "sf64_ps3/boot.h"
#include "sf64_ps3/display.h"
#include "sf64_ps3/arwing.h"

#include <string.h>

enum { BOOT = 100, BOOT_WAIT, SHOW_LOGO, CHECK_SAVE, LOGO_WAIT, START, INIT = 1 };

void sf64_boot_init(Sf64BootState *boot)
{
    memset(boot, 0, sizeof(*boot));
    boot->state = BOOT;
}

int sf64_boot_update(Sf64BootState *boot, uint64_t now_us)
{
    int visible = 0;

    if (boot->state == LOGO_WAIT && now_us >= boot->logo_deadline_us) {
        boot->state = START;
    }
    if (boot->next_state_timer > 0) boot->next_state_timer--;
    switch (boot->state) {
    case BOOT:
        boot->next_state_timer = 2;
        boot->state = BOOT_WAIT;
        break;
    case BOOT_WAIT:
        if (boot->next_state_timer == 0) boot->state = SHOW_LOGO;
        break;
    case SHOW_LOGO:
        visible = 1;
        boot->state = CHECK_SAVE;
        break;
    case CHECK_SAVE:
        boot->save_check_pending = 1;
        boot->logo_deadline_us = now_us + 1000000U;
        boot->state = LOGO_WAIT;
        visible = 1;
        break;
    case LOGO_WAIT:
        visible = 1;
        break;
    case START:
        boot->state = INIT;
        break;
    default:
        break;
    }
    return visible;
}

static void put16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}

static void put32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static void command(uint8_t *rdram, unsigned *count, uint32_t a, uint32_t b)
{
    put32(rdram + *count * 8U, a);
    put32(rdram + *count * 8U + 4U, b);
    (*count)++;
}

static void matrix(uint8_t *dest, const float values[16])
{
    unsigned i;
    for (i = 0; i < 16; i++) {
        uint32_t fixed = (uint32_t)(int32_t)(values[i] * 65536.0f);
        put16(dest + i * 2U, (uint16_t)(fixed >> 16));
        put16(dest + 32U + i * 2U, (uint16_t)fixed);
    }
}

static void vertex(uint8_t *p, int x, int y, int s, int t)
{
    memset(p, 0, 16);
    put16(p, (uint16_t)x);
    put16(p + 2, (uint16_t)y);
    put16(p + 8, (uint16_t)s);
    put16(p + 10, (uint16_t)t);
    p[12] = p[13] = p[14] = p[15] = 255;
}

int sf64_boot_build_task(N64ps3GfxTask *task, uint8_t *rdram, uint32_t size,
                         const uint8_t *logo, uint32_t logo_size, int visible)
{
    float sx, sy;
    float ortho[16] = {0};
    float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    unsigned count = 0, strip, pixel;

    if (!task || !rdram || size < SF64_BOOT_RDRAM_BYTES ||
        (visible && (!logo || logo_size < SF64_BOOT_LOGO_BYTES))) return -1;
    memset(rdram, 0, SF64_BOOT_RDRAM_BYTES);
    if (visible) {
        sf64_display_fit_4_3(sf64_display_aspect(), &sx, &sy);
        ortho[0] = 2.0f / 320.0f * sx;
        ortho[5] = -2.0f / 240.0f * sy;
        ortho[10] = 1.0f;
        ortho[12] = -sx;
        ortho[13] = sy;
        ortho[15] = 1.0f;
        matrix(rdram + 0x200, ortho);
        matrix(rdram + 0x240, identity);
        for (pixel = 0; pixel < SF64_BOOT_LOGO_BYTES; pixel++) {
            uint8_t ia = logo[pixel];
            uint8_t intensity = (uint8_t)((ia >> 4) * 17U);
            uint8_t alpha = (uint8_t)((ia & 15U) * 17U);
            uint8_t *rgba = rdram + 0x1000U + pixel * 4U;
            rgba[0] = rgba[1] = rgba[2] = intensity;
            rgba[3] = alpha;
        }
        command(rdram, &count, 0x01060040U, 0x80000200U);
        command(rdram, &count, 0x01020040U, 0x80000240U);
        command(rdram, &count, 0xb6000000U, 0x00023001U);
        command(rdram, &count, 0xd7000001U, 0xffffffffU);
        command(rdram, &count, 0xe3000c02U, 0x00002000U);
        command(rdram, &count, 0xb900031dU, 0x00504240U);
        for (strip = 0; strip < 5; strip++) {
            unsigned y = strip * 16U;
            unsigned height = strip == 4 ? 10U : 16U;
            unsigned vo = 0x400U + strip * 64U;
            uint8_t *v = rdram + vo;
            command(rdram, &count, 0xe7000000U, 0);
            command(rdram, &count, 0xfd18007fU, 0x80001000U + y * 128U * 4U);
            command(rdram, &count, 0xf5180000U, 0x07080200U);
            command(rdram, &count, 0xe6000000U, 0);
            command(rdram, &count, 0xf3000000U, 0x07000000U | ((128U * height - 1U) << 12));
            command(rdram, &count, 0xe7000000U, 0);
            command(rdram, &count, 0xf5188000U, 0x00080200U);
            command(rdram, &count, 0xf2000000U, (508U << 12) | ((height - 1U) * 4U));
            vertex(v, 100, 86 + (int)y, 0, 0);
            vertex(v + 16, 228, 86 + (int)y, 128 * 32, 0);
            vertex(v + 32, 228, 86 + (int)(y + height), 128 * 32, (int)height * 32);
            vertex(v + 48, 100, 86 + (int)(y + height), 0, (int)height * 32);
            command(rdram, &count, 0x0400103fU, 0x80000000U | vo);
            command(rdram, &count, 0xbf000000U, 0x00000204U);
            command(rdram, &count, 0xbf000000U, 0x00000406U);
        }
    }
    command(rdram, &count, 0xb8000000U, 0);
    memset(task, 0, sizeof(*task));
    task->kind = N64PS3_GFX_TASK_DISPLAY_LIST;
    task->dram_base = rdram;
    task->dram_size_bytes = size;
    task->display_list = rdram;
    task->display_list_size_bytes = count * 8U;
    task->debug_name = "SF64 Nintendo boot logo";
    return 0;
}

static uint32_t get32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static void opening_texture(uint8_t *dest, const uint8_t *indices,
                            const uint8_t *palette, unsigned pixels)
{
    unsigned i;
    for (i = 0; i < pixels; i++) {
        unsigned index = (i & 1U) ? (indices[i / 2U] & 15U) : (indices[i / 2U] >> 4);
        uint16_t color = ((uint16_t)palette[index * 2U] << 8) | palette[index * 2U + 1U];
        unsigned r = (color >> 11) & 31U, g = (color >> 6) & 31U, b = (color >> 1) & 31U;
        dest[i * 4U] = (uint8_t)((r << 3) | (r >> 2));
        dest[i * 4U + 1U] = (uint8_t)((g << 3) | (g >> 2));
        dest[i * 4U + 2U] = (uint8_t)((b << 3) | (b >> 2));
        dest[i * 4U + 3U] = (color & 1U) ? 255 : 0;
    }
}

int sf64_opening_build_task(N64ps3GfxTask *task, uint8_t *rdram, uint32_t size,
                            const uint8_t *title, uint32_t title_size,
                            const uint8_t *stars, uint32_t stars_size,
                            const Sf64OpeningState *state,
                            const uint8_t *great_fox, uint32_t great_fox_size)
{
    float sx, sy, ortho[16] = {0};
    float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    unsigned count = 0, i;

    if (!task || !rdram || size < SF64_OPENING_RDRAM_BYTES || !title ||
        title_size < 0x148d0U || !state || !stars || stars_size < 8 ||
        stars_size > 0x6000U || (stars_size & 7U) ||
        get32(stars + stars_size - 8U) != 0xb8000000U ||
        (state->show_great_fox && (!great_fox || great_fox_size != 0x11e90U))) return -1;
    memset(rdram, 0, SF64_OPENING_RDRAM_BYTES);
    for (i = 0; i + 8U < stars_size; i += 8U) {
        uint32_t a = get32(stars + i), b = get32(stars + i + 4U);
        if ((a >> 24) == 0xf7U) {
            unsigned rgb = b & 0xffffU, factor = 255U - (unsigned)state->fill_alpha;
            unsigned r = ((rgb >> 11) & 31U) * factor / 255U;
            unsigned g = ((rgb >> 6) & 31U) * factor / 255U;
            unsigned blue = ((rgb >> 1) & 31U) * factor / 255U;
            rgb = (r << 11) | (g << 6) | (blue << 1) | (rgb & 1U);
            b = (rgb << 16) | rgb;
        }
        command(rdram, &count, a, b);
    }
    if (count * 8U + 1024U > 0x8000U) return -1;
    if (state->show_great_fox) {
        float projection[16], modelview[16], direction[3];
        uint8_t *light = rdram + 0x8480U;
        unsigned factor = 255U - (unsigned)state->fill_alpha;
        sf64_great_fox_matrices(state->eye, state->at, state->up, state->great_fox_position, state->light_x,
                                state->light_y, projection, modelview, direction);
        matrix(rdram + 0x8400U, projection);
        matrix(rdram + 0x8440U, modelview);
        memcpy(rdram + 0x18000U, great_fox, great_fox_size);
        light[0] = (uint8_t)(101U * factor / 255U);
        light[1] = (uint8_t)(106U * factor / 255U);
        light[2] = (uint8_t)(92U * factor / 255U);
        light[8] = (uint8_t)(int8_t)direction[0];
        light[9] = (uint8_t)(int8_t)direction[1];
        light[10] = (uint8_t)(int8_t)direction[2];
        light[16] = (uint8_t)(3U * factor / 255U);
        light[17] = (uint8_t)(4U * factor / 255U);
        light[18] = (uint8_t)(10U * factor / 255U);
        command(rdram, &count, 0xbc003806U, 0x80018000U);
        command(rdram, &count, 0x01060040U, 0x80008400U);
        command(rdram, &count, 0x01020040U, 0x80008440U);
        command(rdram, &count, 0xbc000002U, 0x00000040U);
        command(rdram, &count, 0x03860010U, 0x80008480U);
        command(rdram, &count, 0x03880010U, 0x80008490U);
        command(rdram, &count, 0xd7000001U, 0xffffffffU);
        command(rdram, &count, 0xb7000000U, 0x00020001U);
        command(rdram, &count, 0x06000000U, 0x0e000000U);
    }
    sf64_display_fit_4_3(sf64_display_aspect(), &sx, &sy);
    ortho[0] = 2.0f / 320.0f * sx;
    ortho[5] = -2.0f / 240.0f * sy;
    ortho[10] = ortho[15] = 1.0f;
    ortho[12] = -sx;
    ortho[13] = sy;
    matrix(rdram + 0x8000, ortho);
    matrix(rdram + 0x8040, identity);
    if (state->logo_state == 0) {
        opening_texture(rdram + 0x9000, title + 0x14140, title + 0x147c0, 256U * 13U);
        opening_texture(rdram + 0xc400, title + 0x147e0, title + 0x148b0, 32U * 13U);
    } else if (state->logo_state > 0) {
        memcpy(rdram + 0x9000, title + 0x6ea0, 128U * 88U * 2U);
    }
    command(rdram, &count, 0x01060040U, 0x80008000U);
    command(rdram, &count, 0x01020040U, 0x80008040U);
    command(rdram, &count, 0xb6000000U, 0x00023001U);
    command(rdram, &count, 0xd7000001U, 0xffffffffU);
    command(rdram, &count, 0xe3000c02U, 0x00002000U);
    command(rdram, &count, 0xb900031dU, 0x00504240U);
    for (i = 0; i < (state->logo_state < 0 ? 0U : state->logo_state == 0 ? 2U : 6U); i++) {
        if (state->logo_state != 0) {
            unsigned y = i * 16U;
            unsigned height = i == 5U ? 8U : 16U;
            unsigned vo = 0x8100U + i * 64U;
            uint8_t *v = rdram + vo;
            float alpha = state->starfox_alpha;
            unsigned j;
            if (alpha < 0.0f) alpha = 0.0f;
            if (alpha > 255.0f) alpha = 255.0f;
            int x0 = (int)state->logo_x;
            int x1 = (int)(state->logo_x + 128.0f * state->logo_scale_x);
            int y0 = (int)(state->logo_y + y * state->logo_scale_y);
            int y1 = (int)(state->logo_y + (y + height) * state->logo_scale_y);
            vertex(v, x0, y0, 0, 0);
            vertex(v + 16, x1, y0, 128 * 32, 0);
            vertex(v + 32, x1, y1, 128 * 32, (int)height * 32);
            vertex(v + 48, x0, y1, 0, (int)height * 32);
            for (j = 0; j < 4; j++) v[j * 16U + 15U] = (uint8_t)alpha;
            command(rdram, &count, 0xe7000000U, 0);
            command(rdram, &count, 0xfd10007fU, 0x80009000U + y * 128U * 2U);
            command(rdram, &count, 0xf5100000U, 0x07080200U);
            command(rdram, &count, 0xe6000000U, 0);
            command(rdram, &count, 0xf3000000U, 0x07000000U | ((128U * height - 1U) << 12));
            command(rdram, &count, 0xe7000000U, 0);
            command(rdram, &count, 0xf5104000U, 0x00080200U);
            command(rdram, &count, 0xf2000000U, (508U << 12) | ((height - 1U) * 4U));
            command(rdram, &count, 0x0400103fU, 0x80000000U | vo);
            command(rdram, &count, 0xbf000000U, 0x00000204U);
            command(rdram, &count, 0xbf000000U, 0x00000406U);
            continue;
        }
        unsigned width = i ? 32U : 256U;
        unsigned x = i ? 150U : 90U;
        unsigned vo = 0x8100U + i * 64U;
        unsigned tex = i ? 0xc400U : 0x9000U;
        float alpha = i ? state->in_alpha : state->starfox_alpha;
        uint8_t *v = rdram + vo;
        unsigned j;
        if (alpha < 0.0f) alpha = 0.0f;
        if (alpha > 255.0f) alpha = 255.0f;
        vertex(v, x, 110, 0, 0);
        vertex(v + 16, x + width, 110, width * 32, 0);
        vertex(v + 32, x + width, 123, width * 32, 13 * 32);
        vertex(v + 48, x, 123, 0, 13 * 32);
        for (j = 0; j < 4; j++) v[j * 16U + 15U] = (uint8_t)alpha;
        command(rdram, &count, 0xe7000000U, 0);
        command(rdram, &count, 0xfd180000U | (width - 1U), 0x80000000U | tex);
        command(rdram, &count, 0xf5180000U, 0x07080200U);
        command(rdram, &count, 0xe6000000U, 0);
        command(rdram, &count, 0xf3000000U, 0x07000000U | ((width * 13U - 1U) << 12));
        command(rdram, &count, 0xe7000000U, 0);
        command(rdram, &count, 0xf5180000U | (((width * 4U + 7U) / 8U) << 9), 0x00080200U);
        command(rdram, &count, 0xf2000000U, (((width - 1U) * 4U) << 12) | (12U * 4U));
        command(rdram, &count, 0x0400103fU, 0x80000000U | vo);
        command(rdram, &count, 0xbf000000U, 0x00000204U);
        command(rdram, &count, 0xbf000000U, 0x00000406U);
    }
    if (state->card_state == 1 && state->show_great_fox) {
        uint8_t *texture = rdram + 0xd000U;
        uint8_t *v = rdram + 0x8280U;
        unsigned j;
        for (j = 0; j < 144U * 28U; j++) {
            uint8_t ia = title[0xe980U + j];
            texture[j * 4U] = texture[j * 4U + 1U] = texture[j * 4U + 2U] = (uint8_t)((ia >> 4) * 17U);
            texture[j * 4U + 3U] = (uint8_t)((ia & 15U) * 17U);
        }
        vertex(v, 89, 17, 0, 0);
        vertex(v + 16, 233, 17, 144 * 32, 0);
        vertex(v + 32, 233, 45, 144 * 32, 28 * 32);
        vertex(v + 48, 89, 45, 0, 28 * 32);
        command(rdram, &count, 0xe7000000U, 0);
        command(rdram, &count, 0xfd18008fU, 0x8000d000U);
        command(rdram, &count, 0xf5180000U, 0x07080200U);
        command(rdram, &count, 0xe6000000U, 0);
        command(rdram, &count, 0xf3000000U, 0x07000000U | ((144U * 28U - 1U) << 12));
        command(rdram, &count, 0xe7000000U, 0);
        command(rdram, &count, 0xf5189000U, 0x00080200U);
        command(rdram, &count, 0xf2000000U, (572U << 12) | (27U * 4U));
        command(rdram, &count, 0x0400103fU, 0x80008280U);
        command(rdram, &count, 0xbf000000U, 0x00000204U);
        command(rdram, &count, 0xbf000000U, 0x00000406U);
    }
    command(rdram, &count, 0xb8000000U, 0);
    memset(task, 0, sizeof(*task));
    task->kind = N64PS3_GFX_TASK_DISPLAY_LIST;
    task->dram_base = rdram;
    task->dram_size_bytes = size;
    task->display_list = rdram;
    task->display_list_size_bytes = count * 8U;
    task->debug_name = "SF64 opening logos and starfield";
    return 0;
}

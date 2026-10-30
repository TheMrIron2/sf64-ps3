#include "sf64_ps3/title_ui.h"

#include "sf64_ps3/arwing.h"
#include "sf64_ps3/display.h"

#include <string.h>

#define SF64_TITLE_UI_MAX_LOGO_STRIPS 30U
#define SF64_TITLE_UI_SEGMENT_OFFSET 0x00030000U
#define SF64_TITLE_UI_SEGMENT_ADDRESS 0x80030000U
#define SF64_TITLE_UI_VERTEX_OFFSET 0x0007d000U
#define SF64_TITLE_UI_ORTHO_OFFSET 0x0007e000U
#define SF64_TITLE_UI_IDENTITY_OFFSET 0x0007e040U
#define SF64_TITLE_UI_N64_PROJECTION_OFFSET 0x0007e080U
#define SF64_TITLE_UI_N64_MODELVIEW_OFFSET 0x0007e0c0U
#define SF64_TITLE_UI_N64_VERTEX_OFFSET 0x0007d400U
#define SF64_TITLE_UI_N64_TEXTURE_OFFSET 0x00080000U
#define SF64_TITLE_UI_N64_TEXTURE_END_OFFSET \
    (SF64_TITLE_UI_N64_TEXTURE_OFFSET + 7U * 4096U)
#define SF64_TITLE_UI_PRESS_START_TEXTURE_OFFSET 0x00088000U
#define SF64_TITLE_UI_PRESS_START_TEXTURE_SEGMENT_OFFSET \
    (SF64_TITLE_UI_PRESS_START_TEXTURE_OFFSET - \
     SF64_TITLE_UI_N64_TEXTURE_OFFSET)
#define SF64_TITLE_UI_TEAM_LIGHT_OFFSET 0x0008b000U
#define SF64_TITLE_UI_PRESS_START_VERTEX_OFFSET 0x0007dc00U
#define SF64_TITLE_UI_ROOT_OFFSET 0x00000100U
#define SF64_TITLE_UI_MATRIX_LIMIT 0x00007000U
#define SF64_TITLE_UI_WIDTH 320.0f
#define SF64_TITLE_UI_HEIGHT 240.0f
#define SF64_TITLE_UI_GFX_BYTES 8U
#define SF64_TITLE_UI_VERTEX_BYTES 16U
#define SF64_TITLE_UI_TITLE_LOGO_OFFSET 0U
#define SF64_TITLE_UI_LOGO_WIDTH 236U
#define SF64_TITLE_UI_LOGO_STRIP_HEIGHT 2U
#define SF64_TITLE_UI_LOGO_STRIP_COUNT 30U
#define SF64_TITLE_UI_LOGO_CHUNK_HEIGHT 6U
#define SF64_TITLE_UI_LOGO_ROWS_PER_CHUNK \
    (SF64_TITLE_UI_LOGO_CHUNK_HEIGHT / SF64_TITLE_UI_LOGO_STRIP_HEIGHT)
#define SF64_TITLE_UI_LOGO_CHUNK_COUNT \
    ((SF64_TITLE_UI_LOGO_STRIP_COUNT + \
      SF64_TITLE_UI_LOGO_ROWS_PER_CHUNK - 1U) / \
     SF64_TITLE_UI_LOGO_ROWS_PER_CHUNK)
#define SF64_TITLE_UI_GFX_ENDDL 0xb8000000U
#define SF64_TITLE_UI_GFX_MOVEWORD_SEGMENT6 0xbc001806U
#define SF64_TITLE_UI_GFX_MTX_PROJECTION_LOAD 0x01060040U
#define SF64_TITLE_UI_GFX_MTX_MODELVIEW_LOAD 0x01020040U
#define SF64_TITLE_UI_GFX_CLEARGEOMETRYMODE 0xb6000000U
#define SF64_TITLE_UI_GFX_TEXTURE 0xd7000001U
#define SF64_TITLE_UI_GFX_MOVEWORD_SEGMENT 0xbc000006U
#define SF64_TITLE_UI_GFX_SETOTHERMODE_L 0xb900031dU
#define SF64_TITLE_UI_GFX_SETOTHERMODE_H_TEXTURE_FILTER 0xe3000c02U
#define SF64_TITLE_UI_TEXTURE_FILTER_POINT 0x00000000U
#define SF64_TITLE_UI_TEXTURE_FILTER_BILERP 0x00002000U
#define SF64_TITLE_UI_RENDER_MODE_XLU_SURFACE 0x00504240U
#define SF64_TITLE_UI_GFX_PIPESYNC 0xe7000000U
#define SF64_TITLE_UI_GFX_LOADSYNC 0xe6000000U
#define SF64_TITLE_UI_GFX_SETTIMG_RGBA16 0xfd100000U
#define SF64_TITLE_UI_GFX_SETTILE_RGBA16 0xf5100000U
#define SF64_TITLE_UI_GFX_SETTIMG_RGBA32 0xfd180000U
#define SF64_TITLE_UI_GFX_SETTILE_RGBA32 0xf5180000U
#define SF64_TITLE_UI_GFX_LOADBLOCK 0xf3000000U
#define SF64_TITLE_UI_GFX_SETTILESIZE 0xf2000000U
#define SF64_TITLE_UI_GFX_SETTIMG 0xfd100000U
#define SF64_TITLE_UI_GFX_VTX 0x04000000U
#define SF64_TITLE_UI_GFX_TRI1 0xbf000000U
#define SF64_TITLE_UI_CLEAR_GEOMETRY_BITS 0x00023001U
#define SF64_TITLE_UI_TEXTURE_TILE_LOAD 7U
#define SF64_TITLE_UI_TEXTURE_CLAMP_S (2U << 8)
#define SF64_TITLE_UI_TEXTURE_CLAMP_T (2U << 18)
#define SF64_TITLE_UI_TEXTURE_CLAMP_ST \
    (SF64_TITLE_UI_TEXTURE_CLAMP_S | SF64_TITLE_UI_TEXTURE_CLAMP_T)
#define SF64_TITLE_UI_TEXTURE_ROW_WORDS \
    ((SF64_TITLE_UI_LOGO_WIDTH * 2U + 7U) / 8U)
#define SF64_TITLE_UI_TEXTURE_LAST_S \
    ((SF64_TITLE_UI_LOGO_WIDTH - 1U) * 4U)
#define SF64_TITLE_UI_PRESS_START_WIDTH 120U
#define SF64_TITLE_UI_PRESS_START_HEIGHT 13U
#define SF64_TITLE_UI_GFX_SETPRIMCOLOR 0xfa000000U

typedef struct Sf64TitleUiRect {
    uint32_t texture_offset;
    uint32_t width;
    uint32_t height;
    float x;
    float y;
    float x_scale;
    float y_scale;
} Sf64TitleUiRect;

typedef float Sf64TitleUiMatrix[4][4];

static const uint32_t s_n64_logo_texture_offsets[7] = {
    0x00014de8U, 0x00015718U, 0x00015f78U, 0x00016908U,
    0x00017208U, 0x00017b18U, 0x000184a8U,
};
static const uint32_t s_n64_logo_tlut_offsets[7] = {
    0x000155e8U, 0x00015ef8U, 0x00016778U, 0x000170e8U,
    0x00017a08U, 0x00018318U, 0x00018ca8U,
};
static const uint16_t s_n64_logo_tlut_counts[7] = {
    152U, 64U, 200U, 144U, 136U, 200U, 76U,
};
static const uint8_t s_n64_logo_texture_heights[7] = {
    64U, 63U, 64U, 63U, 64U, 64U, 64U,
};
static const int16_t s_n64_logo_vertices[7][4][3] = {
    {{-10,0,-20},{-20,0,0},{-10,0,0},{-20,0,-20}},
    {{-20,0,20},{-10,0,20},{-10,0,0},{-20,0,0}},
    {{0,0,0},{0,0,-20},{-10,0,-20},{-10,0,0}},
    {{-10,0,20},{0,0,20},{0,0,0},{-10,0,0}},
    {{0,0,-20},{10,0,0},{10,0,-20},{0,0,0}},
    {{0,0,0},{10,0,20},{10,0,0},{0,0,20}},
    {{10,0,20},{20,0,20},{20,0,0},{10,0,0}},
};
static const int16_t s_n64_logo_texcoords[7][4][2] = {
    {{1024,0},{0,2048},{1024,2048},{0,0}},
    {{0,2016},{1024,2016},{1024,0},{0,0}},
    {{1024,2048},{1024,0},{0,0},{0,2048}},
    {{0,2016},{1024,2016},{1024,0},{0,0}},
    {{0,0},{1024,2048},{1024,0},{0,2048}},
    {{0,0},{1024,2048},{1024,0},{0,2048}},
    {{0,2048},{1024,2048},{1024,0},{0,0}},
};
/* The original aTitle64LogoDL alternates triangle winding/order by panel. */
static const uint8_t s_n64_logo_triangles[7][2][3] = {
    {{3U, 1U, 2U}, {3U, 2U, 0U}},
    {{1U, 2U, 3U}, {1U, 3U, 0U}},
    {{1U, 2U, 3U}, {1U, 3U, 0U}},
    {{1U, 2U, 3U}, {1U, 3U, 0U}},
    {{3U, 1U, 2U}, {3U, 2U, 0U}},
    {{3U, 1U, 2U}, {3U, 2U, 0U}},
    {{1U, 2U, 3U}, {1U, 3U, 0U}},
};

static Sf64TitleUiRect s_logo_rects[SF64_TITLE_UI_MAX_LOGO_STRIPS];
static uint32_t s_logo_rect_count;
static int s_logo_capture_error;
static uint32_t s_press_start_texture_offset;
static uint32_t s_press_start_rect_count;
static uint8_t s_press_start_color[4] = {255U, 255U, 255U, 255U};
static const char *s_title_ui_error = "none";

void sf64_title_ia8_rect(void *gfx_pointer,
                         void *texture_pointer,
                         uint32_t width,
                         uint32_t height,
                         float x,
                         float y,
                         float x_scale,
                         float y_scale)
{
    uintptr_t address = (uintptr_t)texture_pointer;

    (void)gfx_pointer;
    (void)y;
    if ((address >> 24) != 0x06U || width != SF64_TITLE_UI_PRESS_START_WIDTH ||
        (height != 6U && height != 1U) || x != 101.0f ||
        x_scale != 1.0f || y_scale != 1.0f) {
        return;
    }
    if (s_press_start_rect_count == 0U) {
        s_press_start_texture_offset = (uint32_t)(address & 0x00ffffffU);
    }
    s_press_start_rect_count++;
}

void sf64_title_prim_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    s_press_start_color[0] = r;
    s_press_start_color[1] = g;
    s_press_start_color[2] = b;
    s_press_start_color[3] = a;
}

static void write_be16(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)(value >> 8);
    destination[1] = (uint8_t)value;
}

static void write_be32(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)(value >> 24);
    destination[1] = (uint8_t)(value >> 16);
    destination[2] = (uint8_t)(value >> 8);
    destination[3] = (uint8_t)value;
}

static uint32_t read_be32(const uint8_t *source)
{
    return ((uint32_t)source[0] << 24) |
           ((uint32_t)source[1] << 16) |
           ((uint32_t)source[2] << 8) |
           (uint32_t)source[3];
}

static void append_command(uint8_t *rdram,
                           uint32_t *command_count,
                           uint32_t word0,
                           uint32_t word1)
{
    uint8_t *command = rdram + SF64_TITLE_UI_ROOT_OFFSET +
                       *command_count * SF64_TITLE_UI_GFX_BYTES;

    write_be32(command, word0);
    write_be32(command + 4U, word1);
    (*command_count)++;
}

static void matrix_write_n64(uint8_t *destination,
                             Sf64TitleUiMatrix matrix)
{
    uint32_t row;
    uint32_t column;

    for (row = 0; row < 4U; row++) {
        for (column = 0; column < 4U; column++) {
            int32_t fixed = (int32_t)(matrix[row][column] * 65536.0f);
            uint32_t encoded = (uint32_t)fixed;
            uint32_t index = row * 4U + column;

            write_be16(destination + index * 2U,
                       (uint16_t)(encoded >> 16));
            write_be16(destination + 32U + index * 2U,
                       (uint16_t)encoded);
        }
    }
}

static void make_ortho_matrix(Sf64TitleUiMatrix matrix)
{
    float scale_x;
    float scale_y;

    sf64_display_fit_4_3(sf64_display_aspect(), &scale_x, &scale_y);

    memset(matrix, 0, sizeof(Sf64TitleUiMatrix));
    matrix[0][0] = (2.0f / SF64_TITLE_UI_WIDTH) * scale_x;
    matrix[1][1] = (-2.0f / SF64_TITLE_UI_HEIGHT) * scale_y;
    matrix[2][2] = 1.0f;
    matrix[3][0] = -scale_x;
    matrix[3][1] = scale_y;
    matrix[3][3] = 1.0f;
}

static void make_identity_matrix(Sf64TitleUiMatrix matrix)
{
    uint32_t axis;

    memset(matrix, 0, sizeof(Sf64TitleUiMatrix));
    for (axis = 0; axis < 4U; axis++) {
        matrix[axis][axis] = 1.0f;
    }
}

static void write_vertex(uint8_t *destination,
                         int16_t x,
                         int16_t y,
                         int16_t s,
                         int16_t t)
{
    write_be16(destination, (uint16_t)x);
    write_be16(destination + 2U, (uint16_t)y);
    write_be16(destination + 4U, 0U);
    write_be16(destination + 6U, 0U);
    write_be16(destination + 8U, (uint16_t)s);
    write_be16(destination + 10U, (uint16_t)t);
    destination[12] = 255U;
    destination[13] = 255U;
    destination[14] = 255U;
    destination[15] = 255U;
}

static void make_n64_logo_matrices(Sf64TitleUiMatrix projection,
                                   Sf64TitleUiMatrix modelview,
                                   const float position[3])
{
    const float near_plane = 10.0f;
    const float far_plane = 12800.0f;
    const float cotangent = 2.41421356237f;
    const float reference_aspect = SF64_DISPLAY_REFERENCE_ASPECT;
    float scale_x;
    float scale_y;
    uint32_t column;

    sf64_display_fit_4_3(sf64_display_aspect(), &scale_x, &scale_y);

    memset(projection, 0, sizeof(Sf64TitleUiMatrix));
    projection[0][0] = (cotangent / reference_aspect) * scale_x;
    projection[1][1] = cotangent * scale_y;
    projection[2][2] = (near_plane + far_plane) / (near_plane - far_plane);
    projection[2][3] = -1.0f;
    projection[3][2] = (2.0f * near_plane * far_plane) /
                       (near_plane - far_plane);

    make_identity_matrix(modelview);
    modelview[0][0] = 1.5f;
    modelview[1][1] = 3.0f;
    for (column = 0; column < 4U; column++) {
        float y = modelview[1][column];
        float z = modelview[2][column];
        modelview[1][column] = z;
        modelview[2][column] = -y;
    }
    modelview[3][0] = position[0];
    modelview[3][1] = position[1] - 180.0f;
    modelview[3][2] = position[2] - 430.0f;
}

static void write_n64_logo_vertex(uint8_t *destination,
                                  const int16_t position[3],
                                  int16_t s,
                                  int16_t t)
{
    write_be16(destination, (uint16_t)position[0]);
    write_be16(destination + 2U, (uint16_t)position[1]);
    write_be16(destination + 4U, (uint16_t)position[2]);
    write_be16(destination + 6U, 0U);
    write_be16(destination + 8U, (uint16_t)s);
    write_be16(destination + 10U, (uint16_t)t);
    destination[12] = 255U;
    destination[13] = 255U;
    destination[14] = 255U;
    destination[15] = 255U;
}

static int build_n64_logo_textures(uint8_t *rdram,
                                   const uint8_t *title_segment,
                                   uint32_t *opaque_texel_count)
{
    uint32_t texture_index;
    uint32_t opaque_count = 0U;

    for (texture_index = 0; texture_index < 7U; texture_index++) {
        uint32_t pixel_count = 32U * s_n64_logo_texture_heights[texture_index];
        const uint8_t *indices = title_segment + s_n64_logo_texture_offsets[texture_index];
        const uint8_t *palette = title_segment + s_n64_logo_tlut_offsets[texture_index];
        uint8_t *rgba16 = rdram + SF64_TITLE_UI_N64_TEXTURE_OFFSET + texture_index * 4096U;
        uint32_t pixel;

        for (pixel = 0; pixel < pixel_count; pixel++) {
            uint32_t palette_index = indices[pixel];
            if (palette_index >= s_n64_logo_tlut_counts[texture_index]) {
                return -1;
            }
            write_be16(rgba16 + pixel * 2U,
                       (uint16_t)(((uint16_t)palette[palette_index * 2U] << 8) |
                                  palette[palette_index * 2U + 1U]));
            opaque_count += palette[palette_index * 2U + 1U] & 1U;
        }
    }
    if (opaque_texel_count) {
        *opaque_texel_count = opaque_count;
    }
    return 0;
}

static void append_n64_logo(uint8_t *rdram,
                            uint32_t *command_count,
                            const float position[3])
{
    Sf64TitleUiMatrix projection;
    Sf64TitleUiMatrix modelview;
    uint32_t texture_index;

    make_n64_logo_matrices(projection, modelview, position);
    matrix_write_n64(rdram + SF64_TITLE_UI_N64_PROJECTION_OFFSET, projection);
    matrix_write_n64(rdram + SF64_TITLE_UI_N64_MODELVIEW_OFFSET, modelview);
    append_command(rdram, command_count,
                   SF64_TITLE_UI_GFX_MOVEWORD_SEGMENT | ((7U * 4U) << 8),
                   0x80080000U);
    append_command(rdram, command_count, SF64_TITLE_UI_GFX_MTX_PROJECTION_LOAD,
                   0x80000000U | SF64_TITLE_UI_N64_PROJECTION_OFFSET);
    append_command(rdram, command_count, SF64_TITLE_UI_GFX_MTX_MODELVIEW_LOAD,
                   0x80000000U | SF64_TITLE_UI_N64_MODELVIEW_OFFSET);
    append_command(rdram, command_count, SF64_TITLE_UI_GFX_CLEARGEOMETRYMODE,
                   SF64_TITLE_UI_CLEAR_GEOMETRY_BITS);
    append_command(rdram, command_count, SF64_TITLE_UI_GFX_TEXTURE, 0xffffffffU);
    append_command(rdram, command_count,
                   SF64_TITLE_UI_GFX_SETOTHERMODE_H_TEXTURE_FILTER,
                   SF64_TITLE_UI_TEXTURE_FILTER_BILERP);
    append_command(rdram, command_count, SF64_TITLE_UI_GFX_SETOTHERMODE_L,
                   SF64_TITLE_UI_RENDER_MODE_XLU_SURFACE);

    for (texture_index = 0; texture_index < 7U; texture_index++) {
        uint32_t height = s_n64_logo_texture_heights[texture_index];
        uint32_t texture_offset = texture_index * 4096U;
        uint32_t vertex_offset = SF64_TITLE_UI_N64_VERTEX_OFFSET + texture_index * 64U;
        uint8_t *vertices = rdram + vertex_offset;
        uint32_t vertex;

        append_command(rdram, command_count, SF64_TITLE_UI_GFX_PIPESYNC, 0U);
        append_command(rdram, command_count, SF64_TITLE_UI_GFX_SETTIMG | 31U,
                       0x07000000U | texture_offset);
        append_command(rdram, command_count, SF64_TITLE_UI_GFX_SETTILE_RGBA16,
                       (SF64_TITLE_UI_TEXTURE_TILE_LOAD << 24) |
                           SF64_TITLE_UI_TEXTURE_CLAMP_ST);
        append_command(rdram, command_count, SF64_TITLE_UI_GFX_LOADSYNC, 0U);
        append_command(rdram, command_count, SF64_TITLE_UI_GFX_LOADBLOCK,
                       (SF64_TITLE_UI_TEXTURE_TILE_LOAD << 24) |
                           ((32U * height - 1U) << 12));
        append_command(rdram, command_count, SF64_TITLE_UI_GFX_PIPESYNC, 0U);
        append_command(rdram, command_count,
                       SF64_TITLE_UI_GFX_SETTILE_RGBA16 | (8U << 9),
                       SF64_TITLE_UI_TEXTURE_CLAMP_ST);
        append_command(rdram, command_count, SF64_TITLE_UI_GFX_SETTILESIZE,
                       (124U << 12) | ((height - 1U) * 4U));
        for (vertex = 0; vertex < 4U; vertex++) {
            write_n64_logo_vertex(vertices + vertex * 16U,
                                  s_n64_logo_vertices[texture_index][vertex],
                                  s_n64_logo_texcoords[texture_index][vertex][0],
                                  s_n64_logo_texcoords[texture_index][vertex][1]);
        }
        append_command(rdram, command_count, SF64_TITLE_UI_GFX_VTX | (4U << 10) | 63U,
                       0x80000000U | vertex_offset);
        {
            uint32_t triangle;

            for (triangle = 0; triangle < 2U; triangle++) {
                const uint8_t *indices =
                    s_n64_logo_triangles[texture_index][triangle];
                uint32_t encoded_indices =
                    ((uint32_t)indices[0] * 2U << 16) |
                    ((uint32_t)indices[1] * 2U << 8) |
                    ((uint32_t)indices[2] * 2U);

                append_command(rdram, command_count,
                               SF64_TITLE_UI_GFX_TRI1, encoded_indices);
            }
        }
    }
}

static int append_press_start(uint8_t *rdram,
                              uint32_t *command_count,
                              const uint8_t *title_segment,
                              uint32_t title_segment_size)
{
    uint8_t *texture = rdram + SF64_TITLE_UI_PRESS_START_TEXTURE_OFFSET;
    uint8_t *vertices = rdram + SF64_TITLE_UI_PRESS_START_VERTEX_OFFSET;
    uint32_t pixel;

    if (s_press_start_rect_count == 0U) {
        return 1;
    }
    if (s_press_start_rect_count != 3U ||
        s_press_start_texture_offset > title_segment_size ||
        SF64_TITLE_UI_PRESS_START_WIDTH * SF64_TITLE_UI_PRESS_START_HEIGHT >
            title_segment_size - s_press_start_texture_offset ||
        SF64_TITLE_UI_PRESS_START_TEXTURE_OFFSET <
            SF64_TITLE_UI_N64_TEXTURE_END_OFFSET ||
        SF64_TITLE_UI_PRESS_START_TEXTURE_OFFSET +
                SF64_TITLE_UI_PRESS_START_WIDTH *
                    SF64_TITLE_UI_PRESS_START_HEIGHT * 4U >
            SF64_TITLE_UI_TEAM_LIGHT_OFFSET) {
        return -1;
    }

    for (pixel = 0; pixel < SF64_TITLE_UI_PRESS_START_WIDTH *
                               SF64_TITLE_UI_PRESS_START_HEIGHT;
         pixel++) {
        uint8_t ia8 = title_segment[s_press_start_texture_offset + pixel];
        uint8_t intensity = (uint8_t)((ia8 >> 4) * 17U);
        uint8_t alpha = (uint8_t)((ia8 & 0x0fU) * 17U);
        uint32_t rgba32;

        /* Match the PSP IA8 soft-coverage transfer before RSX bilinear
         * filtering. IA8 has 4-bit alpha; preserve it as 8-bit coverage. */
        if (alpha <= 32U) {
            alpha = 0U;
        } else {
            alpha = (uint8_t)((((uint32_t)(alpha - 32U) * 255U) + 111U) /
                              223U);
        }
        rgba32 = ((uint32_t)intensity << 24) |
                 ((uint32_t)intensity << 16) |
                 ((uint32_t)intensity << 8) | alpha;

        write_be32(texture + pixel * 4U, rgba32);
    }

    append_command(rdram, command_count, SF64_TITLE_UI_GFX_PIPESYNC, 0U);
    /* The original team display lists can leave opaque mode active. Preserve
     * the IA8 mask by restoring the translucent renderer state for this quad. */
    append_command(rdram, command_count,
                   SF64_TITLE_UI_GFX_SETOTHERMODE_L,
                   SF64_TITLE_UI_RENDER_MODE_XLU_SURFACE);
    append_command(rdram, command_count,
                   SF64_TITLE_UI_GFX_SETOTHERMODE_H_TEXTURE_FILTER,
                   SF64_TITLE_UI_TEXTURE_FILTER_POINT);
    append_command(rdram, command_count,
                   SF64_TITLE_UI_GFX_MOVEWORD_SEGMENT | ((7U * 4U) << 8),
                   0x80080000U);
    append_command(rdram, command_count, SF64_TITLE_UI_GFX_SETPRIMCOLOR,
                   ((uint32_t)s_press_start_color[0] << 24) |
                       ((uint32_t)s_press_start_color[1] << 16) |
                       ((uint32_t)s_press_start_color[2] << 8) |
                       s_press_start_color[3]);
    append_command(rdram, command_count,
                   SF64_TITLE_UI_GFX_SETTIMG_RGBA32 |
                       (SF64_TITLE_UI_PRESS_START_WIDTH - 1U),
                   0x07000000U |
                       SF64_TITLE_UI_PRESS_START_TEXTURE_SEGMENT_OFFSET);
    append_command(rdram, command_count,
                   SF64_TITLE_UI_GFX_SETTILE_RGBA32,
                   (SF64_TITLE_UI_TEXTURE_TILE_LOAD << 24) |
                       SF64_TITLE_UI_TEXTURE_CLAMP_ST);
    append_command(rdram, command_count, SF64_TITLE_UI_GFX_LOADSYNC, 0U);
    append_command(rdram, command_count,
                   SF64_TITLE_UI_GFX_LOADBLOCK,
                   (SF64_TITLE_UI_TEXTURE_TILE_LOAD << 24) |
                       ((SF64_TITLE_UI_PRESS_START_WIDTH *
                             SF64_TITLE_UI_PRESS_START_HEIGHT -
                         1U)
                        << 12));
    append_command(rdram, command_count, SF64_TITLE_UI_GFX_PIPESYNC, 0U);
    append_command(rdram, command_count,
                   SF64_TITLE_UI_GFX_SETTILE_RGBA32 |
                       (((SF64_TITLE_UI_PRESS_START_WIDTH * 4U + 7U) / 8U)
                        << 9),
                   SF64_TITLE_UI_TEXTURE_CLAMP_ST);
    append_command(rdram, command_count,
                   SF64_TITLE_UI_GFX_SETTILESIZE,
                   (((SF64_TITLE_UI_PRESS_START_WIDTH - 1U) * 4U) << 12) |
                       ((SF64_TITLE_UI_PRESS_START_HEIGHT - 1U) * 4U));
    write_vertex(vertices, 101, 188, 0, 0);
    write_vertex(vertices + SF64_TITLE_UI_VERTEX_BYTES, 221, 188,
                 120 * 32, 0);
    write_vertex(vertices + 2U * SF64_TITLE_UI_VERTEX_BYTES, 221, 201,
                 120 * 32, 13 * 32);
    write_vertex(vertices + 3U * SF64_TITLE_UI_VERTEX_BYTES, 101, 201,
                 0, 13 * 32);
    for (pixel = 0; pixel < 4U; pixel++) {
        uint8_t *vertex = vertices + pixel * SF64_TITLE_UI_VERTEX_BYTES;
        vertex[12] = s_press_start_color[0];
        vertex[13] = s_press_start_color[1];
        vertex[14] = s_press_start_color[2];
        vertex[15] = s_press_start_color[3];
    }
    append_command(rdram, command_count,
                   SF64_TITLE_UI_GFX_VTX | (4U << 10) | 63U,
                   0x80000000U | SF64_TITLE_UI_PRESS_START_VERTEX_OFFSET);
    append_command(rdram, command_count, SF64_TITLE_UI_GFX_TRI1, 0x00000204U);
    append_command(rdram, command_count, SF64_TITLE_UI_GFX_TRI1, 0x00000406U);
    return 0;
}

static int validate_capture(void)
{
    uint32_t index;

    if (s_logo_capture_error ||
        s_logo_rect_count != SF64_TITLE_UI_LOGO_STRIP_COUNT) {
        return -1;
    }

    for (index = 0; index < s_logo_rect_count; index++) {
        const Sf64TitleUiRect *rect = &s_logo_rects[index];
        uint32_t expected_offset = SF64_TITLE_UI_TITLE_LOGO_OFFSET +
                                   index * SF64_TITLE_UI_LOGO_WIDTH *
                                       SF64_TITLE_UI_LOGO_STRIP_HEIGHT * 2U;
        float expected_y = 37.0f + (float)(index * 2U);

        if (rect->texture_offset != expected_offset ||
            rect->width != SF64_TITLE_UI_LOGO_WIDTH ||
            rect->height != SF64_TITLE_UI_LOGO_STRIP_HEIGHT ||
            rect->x != 26.0f || rect->y != expected_y ||
            rect->x_scale != 1.0f || rect->y_scale != 1.0f) {
            return -1;
        }
    }
    return 0;
}

void sf64_title_ui_reset(void)
{
    memset(s_logo_rects, 0, sizeof(s_logo_rects));
    s_logo_rect_count = 0U;
    s_logo_capture_error = 0;
    s_press_start_texture_offset = 0U;
    s_press_start_rect_count = 0U;
    s_press_start_color[0] = 255U;
    s_press_start_color[1] = 255U;
    s_press_start_color[2] = 255U;
    s_press_start_color[3] = 255U;
    s_title_ui_error = "none";
}

const char *sf64_title_ui_last_error(void)
{
    return s_title_ui_error;
}

void sf64_title_logo_rect(void *gfx_pointer,
                          void *texture_pointer,
                          uint32_t width,
                          uint32_t height,
                          float x,
                          float y,
                          float x_scale,
                          float y_scale)
{
    uintptr_t address = (uintptr_t)texture_pointer;
    Sf64TitleUiRect *rect;

    (void)gfx_pointer;
    if (s_logo_rect_count >= SF64_TITLE_UI_MAX_LOGO_STRIPS ||
        (address >> 24) != 0x06U) {
        s_logo_capture_error = 1;
        return;
    }

    rect = &s_logo_rects[s_logo_rect_count++];
    rect->texture_offset = (uint32_t)(address & 0x00ffffffU);
    rect->width = width;
    rect->height = height;
    rect->x = x;
    rect->y = y;
    rect->x_scale = x_scale;
    rect->y_scale = y_scale;
}

int sf64_title_ui_append_logo(N64ps3GfxTask *task,
                              const uint8_t *title_segment,
                              uint32_t title_segment_size,
                              const float n64_logo_position[3],
                              Sf64TitleUiInfo *info)
{
    uint8_t *rdram;
    uint32_t command_count;
    uint32_t original_command_count;
    uint32_t end_offset;
    uint32_t n64_logo_opaque_texels = 0U;
    Sf64TitleUiMatrix ortho;
    Sf64TitleUiMatrix identity;
    uint32_t index;

    if (!task || !task->dram_base || !task->display_list || !title_segment ||
        !n64_logo_position) {
        s_title_ui_error = "null append argument";
        return -1;
    }
    if (task->kind != N64PS3_GFX_TASK_DISPLAY_LIST ||
        task->dram_size_bytes < SF64_ARWING_RDRAM_REQUIRED_SIZE) {
        s_title_ui_error = "invalid task kind or RDRAM size";
        return -1;
    }
    if (title_segment_size != 0x0004ca30U) {
        s_title_ui_error = "unexpected title segment size";
        return -1;
    }
    if (s_logo_capture_error ||
        s_logo_rect_count != SF64_TITLE_UI_LOGO_STRIP_COUNT) {
        s_title_ui_error = s_logo_capture_error ? "logo capture address/count error"
                                                : "wrong logo strip count";
        return -1;
    }
    if (validate_capture() != 0) {
        s_title_ui_error = "logo strip layout mismatch";
        return -1;
    }
    if (
        SF64_TITLE_UI_SEGMENT_OFFSET + title_segment_size >
            SF64_TITLE_UI_VERTEX_OFFSET ||
        SF64_TITLE_UI_VERTEX_OFFSET +
                SF64_TITLE_UI_LOGO_CHUNK_COUNT * 4U *
                    SF64_TITLE_UI_VERTEX_BYTES >
            SF64_TITLE_UI_ORTHO_OFFSET ||
        SF64_TITLE_UI_N64_TEXTURE_OFFSET + 7U * 4096U > task->dram_size_bytes ||
        SF64_TITLE_UI_N64_MODELVIEW_OFFSET + 64U > task->dram_size_bytes ||
        task->display_list_size_bytes < SF64_TITLE_UI_GFX_BYTES ||
        (task->display_list_size_bytes % SF64_TITLE_UI_GFX_BYTES) != 0U) {
        s_title_ui_error = "RDRAM layout or root-list size invalid";
        return -1;
    }

    rdram = (uint8_t *)(uintptr_t)task->dram_base;
    end_offset = (uint32_t)((const uint8_t *)task->display_list - rdram) +
                 task->display_list_size_bytes;
    if (end_offset != SF64_TITLE_UI_ROOT_OFFSET +
                          task->display_list_size_bytes ||
        end_offset > SF64_TITLE_UI_MATRIX_LIMIT ||
        read_be32(rdram + end_offset - SF64_TITLE_UI_GFX_BYTES) !=
            SF64_TITLE_UI_GFX_ENDDL) {
        s_title_ui_error = "root display-list terminator missing or offset invalid";
        return -1;
    }

    memcpy(rdram + SF64_TITLE_UI_SEGMENT_OFFSET,
           title_segment, title_segment_size);
    if (build_n64_logo_textures(rdram, title_segment,
                                &n64_logo_opaque_texels) != 0) {
        s_title_ui_error = "64 logo texture decode failed";
        return -1;
    }
    make_ortho_matrix(ortho);
    make_identity_matrix(identity);
    matrix_write_n64(rdram + SF64_TITLE_UI_ORTHO_OFFSET, ortho);
    matrix_write_n64(rdram + SF64_TITLE_UI_IDENTITY_OFFSET, identity);

    original_command_count =
        task->display_list_size_bytes / SF64_TITLE_UI_GFX_BYTES;
    if (SF64_TITLE_UI_ROOT_OFFSET +
            (original_command_count - 1U + 6U +
             SF64_TITLE_UI_LOGO_CHUNK_COUNT * 11U + 23U + 6U + 7U * 11U + 1U) *
            SF64_TITLE_UI_GFX_BYTES >
        SF64_TITLE_UI_MATRIX_LIMIT) {
        s_title_ui_error = "title UI exceeds RDRAM matrix region";
        return -1;
    }
    command_count = original_command_count - 1U;
    append_command(rdram, &command_count,
                   SF64_TITLE_UI_GFX_MOVEWORD_SEGMENT6,
                   SF64_TITLE_UI_SEGMENT_ADDRESS);
    append_command(rdram, &command_count,
                   SF64_TITLE_UI_GFX_MTX_PROJECTION_LOAD,
                   0x80000000U | SF64_TITLE_UI_ORTHO_OFFSET);
    append_command(rdram, &command_count,
                   SF64_TITLE_UI_GFX_MTX_MODELVIEW_LOAD,
                   0x80000000U | SF64_TITLE_UI_IDENTITY_OFFSET);
    append_command(rdram, &command_count,
                   SF64_TITLE_UI_GFX_CLEARGEOMETRYMODE,
                   SF64_TITLE_UI_CLEAR_GEOMETRY_BITS);
    append_command(rdram, &command_count,
                   SF64_TITLE_UI_GFX_TEXTURE,
                   0xffffffffU);
    append_command(rdram, &command_count,
                   SF64_TITLE_UI_GFX_SETOTHERMODE_H_TEXTURE_FILTER,
                   SF64_TITLE_UI_TEXTURE_FILTER_BILERP);
    append_command(rdram, &command_count,
                   SF64_TITLE_UI_GFX_SETOTHERMODE_L,
                   SF64_TITLE_UI_RENDER_MODE_XLU_SURFACE);

    for (index = 0; index < SF64_TITLE_UI_LOGO_CHUNK_COUNT; index++) {
        const Sf64TitleUiRect *rect =
            &s_logo_rects[index * SF64_TITLE_UI_LOGO_ROWS_PER_CHUNK];
        uint32_t first_pixel_row = index * SF64_TITLE_UI_LOGO_CHUNK_HEIGHT;
        uint32_t chunk_height = SF64_TITLE_UI_LOGO_CHUNK_HEIGHT;
        uint32_t top_guard_rows;
        uint32_t bottom_guard_rows;
        uint32_t texture_height;
        uint32_t texture_load_count;
        uint32_t texture_last_t;
        uint32_t image_address = 0x06000000U;
        uint32_t vertex_offset = SF64_TITLE_UI_VERTEX_OFFSET +
                                 index * 4U * SF64_TITLE_UI_VERTEX_BYTES;
        float x0 = rect->x;
        float y0 = rect->y;
        float x1 = x0 + (float)rect->width * rect->x_scale;
        float y1;
        int16_t right_s = (int16_t)(rect->width * 32U);
        int16_t top_t;
        int16_t bottom_t;
        uint8_t *vertices = rdram + vertex_offset;
        uint32_t loadblock_word1;
        uint32_t tile_size_word1 = SF64_TITLE_UI_TEXTURE_LAST_S << 12;

        if (first_pixel_row + chunk_height >
            SF64_TITLE_UI_LOGO_STRIP_COUNT *
                SF64_TITLE_UI_LOGO_STRIP_HEIGHT) {
            chunk_height = SF64_TITLE_UI_LOGO_STRIP_COUNT *
                               SF64_TITLE_UI_LOGO_STRIP_HEIGHT -
                           first_pixel_row;
        }
        top_guard_rows = first_pixel_row == 0U ? 0U : 1U;
        bottom_guard_rows =
            first_pixel_row + chunk_height <
                    SF64_TITLE_UI_LOGO_STRIP_COUNT *
                        SF64_TITLE_UI_LOGO_STRIP_HEIGHT
                ? 1U
                : 0U;
        texture_height = chunk_height + top_guard_rows + bottom_guard_rows;
        texture_load_count = SF64_TITLE_UI_LOGO_WIDTH * texture_height;
        texture_last_t = (texture_height - 1U) * 4U;
        image_address |= rect->texture_offset -
                         top_guard_rows * SF64_TITLE_UI_LOGO_WIDTH * 2U;
        loadblock_word1 = (SF64_TITLE_UI_TEXTURE_TILE_LOAD << 24) |
                          ((texture_load_count - 1U) << 12);
        tile_size_word1 |= texture_last_t;
        y1 = y0 + (float)chunk_height * rect->y_scale;
        top_t = (int16_t)(top_guard_rows * 32U);
        bottom_t = (int16_t)((top_guard_rows + chunk_height) * 32U);

        append_command(rdram, &command_count,
                       SF64_TITLE_UI_GFX_PIPESYNC, 0U);
        append_command(rdram, &command_count,
                       SF64_TITLE_UI_GFX_SETTIMG_RGBA16 |
                           (rect->width - 1U),
                       image_address);
        append_command(rdram, &command_count,
                       SF64_TITLE_UI_GFX_SETTILE_RGBA16,
                       (SF64_TITLE_UI_TEXTURE_TILE_LOAD << 24) |
                           SF64_TITLE_UI_TEXTURE_CLAMP_ST);
        append_command(rdram, &command_count,
                       SF64_TITLE_UI_GFX_LOADSYNC, 0U);
        append_command(rdram, &command_count,
                       SF64_TITLE_UI_GFX_LOADBLOCK,
                       loadblock_word1);
        append_command(rdram, &command_count,
                       SF64_TITLE_UI_GFX_PIPESYNC, 0U);
        append_command(rdram, &command_count,
                       SF64_TITLE_UI_GFX_SETTILE_RGBA16 |
                           (SF64_TITLE_UI_TEXTURE_ROW_WORDS << 9),
                       SF64_TITLE_UI_TEXTURE_CLAMP_ST);
        append_command(rdram, &command_count,
                       SF64_TITLE_UI_GFX_SETTILESIZE,
                       tile_size_word1);

        write_vertex(vertices,
                     (int16_t)x0, (int16_t)y0, 0, top_t);
        write_vertex(vertices + SF64_TITLE_UI_VERTEX_BYTES,
                     (int16_t)x1, (int16_t)y0, right_s, top_t);
        write_vertex(vertices + 2U * SF64_TITLE_UI_VERTEX_BYTES,
                     (int16_t)x1, (int16_t)y1, right_s, bottom_t);
        write_vertex(vertices + 3U * SF64_TITLE_UI_VERTEX_BYTES,
                     (int16_t)x0, (int16_t)y1, 0, bottom_t);

        append_command(rdram, &command_count,
                       SF64_TITLE_UI_GFX_VTX |
                           (4U << 10) |
                           (4U * SF64_TITLE_UI_VERTEX_BYTES - 1U),
                       0x80000000U | vertex_offset);
        append_command(rdram, &command_count,
                       SF64_TITLE_UI_GFX_TRI1,
                       0x00000204U);
        append_command(rdram, &command_count,
                       SF64_TITLE_UI_GFX_TRI1,
                       0x00000406U);
    }

    {
        int press_start_result = append_press_start(
            rdram, &command_count, title_segment, title_segment_size);
        if (press_start_result < 0) {
            s_title_ui_error = "PRESS START texture/layout invalid";
            return -1;
        }
        if (info) {
            info->press_start_drawn = press_start_result == 0 ? 1U : 0U;
        }
    }
    append_n64_logo(rdram, &command_count, n64_logo_position);

    append_command(rdram, &command_count,
                   SF64_TITLE_UI_GFX_ENDDL, 0U);
    task->display_list_size_bytes =
        command_count * SF64_TITLE_UI_GFX_BYTES;
    task->debug_name = "SF64 original title starfield, Arwing, and logos";

    if (info) {
        info->logo_strip_count = s_logo_rect_count;
        info->generated_command_count =
            command_count - original_command_count;
        info->n64_logo_panel_count = 7U;
        info->n64_logo_triangle_count = 14U;
        info->n64_logo_opaque_texels = n64_logo_opaque_texels;
        info->n64_logo_position_valid = 1U;
    }
    return 0;
}

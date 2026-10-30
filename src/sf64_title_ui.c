#include "sf64_ps3/title_ui.h"

#include "sf64_ps3/arwing.h"

#include <string.h>

#if defined(N64PS3_PS3)
#include <sysutil/video.h>
#endif

#define SF64_TITLE_UI_MAX_LOGO_STRIPS 30U
#define SF64_TITLE_UI_SEGMENT_OFFSET 0x00030000U
#define SF64_TITLE_UI_SEGMENT_ADDRESS 0x80030000U
#define SF64_TITLE_UI_VERTEX_OFFSET 0x0007d000U
#define SF64_TITLE_UI_ORTHO_OFFSET 0x0007e000U
#define SF64_TITLE_UI_IDENTITY_OFFSET 0x0007e040U
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
#define SF64_TITLE_UI_GFX_SETOTHERMODE_L 0xb900031dU
#define SF64_TITLE_UI_RENDER_MODE_XLU_SURFACE 0x00504240U
#define SF64_TITLE_UI_GFX_PIPESYNC 0xe7000000U
#define SF64_TITLE_UI_GFX_LOADSYNC 0xe6000000U
#define SF64_TITLE_UI_GFX_SETTIMG_RGBA16 0xfd100000U
#define SF64_TITLE_UI_GFX_SETTILE_RGBA16 0xf5100000U
#define SF64_TITLE_UI_GFX_LOADBLOCK 0xf3000000U
#define SF64_TITLE_UI_GFX_SETTILESIZE 0xf2000000U
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

static Sf64TitleUiRect s_logo_rects[SF64_TITLE_UI_MAX_LOGO_STRIPS];
static uint32_t s_logo_rect_count;
static int s_logo_capture_error;
static float s_output_aspect = 4.0f / 3.0f;
static int s_output_aspect_initialized;

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
                             const Sf64TitleUiMatrix matrix)
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

static float get_output_aspect(void)
{
#if defined(N64PS3_PS3)
    if (!s_output_aspect_initialized) {
        videoState state;
        videoResolution resolution;

        memset(&state, 0, sizeof(state));
        if (videoGetState(VIDEO_PRIMARY, 0, &state) == 0 &&
            videoGetResolution(state.displayMode.resolution, &resolution) == 0 &&
            resolution.width != 0U && resolution.height != 0U) {
            if (state.displayMode.aspect == VIDEO_ASPECT_16_9) {
                s_output_aspect = 16.0f / 9.0f;
            } else if (state.displayMode.aspect == VIDEO_ASPECT_4_3) {
                s_output_aspect = 4.0f / 3.0f;
            } else {
                s_output_aspect = (float)resolution.width /
                                  (float)resolution.height;
            }
        }
        s_output_aspect_initialized = 1;
    }
#else
    s_output_aspect_initialized = 1;
#endif
    return s_output_aspect;
}

static void make_ortho_matrix(Sf64TitleUiMatrix matrix)
{
    const float reference_aspect = 4.0f / 3.0f;
    const float output_aspect = get_output_aspect();
    float scale_x = 1.0f;
    float scale_y = 1.0f;

    if (output_aspect > reference_aspect) {
        scale_x = reference_aspect / output_aspect;
    } else if (output_aspect > 0.0f) {
        scale_y = output_aspect / reference_aspect;
    }

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
                              Sf64TitleUiInfo *info)
{
    uint8_t *rdram;
    uint32_t command_count;
    uint32_t original_command_count;
    uint32_t end_offset;
    Sf64TitleUiMatrix ortho;
    Sf64TitleUiMatrix identity;
    uint32_t index;

    if (!task || !task->dram_base || !task->display_list || !title_segment ||
        task->kind != N64PS3_GFX_TASK_DISPLAY_LIST ||
        task->dram_size_bytes < SF64_ARWING_RDRAM_REQUIRED_SIZE ||
        title_segment_size != 0x0004ca30U || validate_capture() != 0 ||
        SF64_TITLE_UI_SEGMENT_OFFSET + title_segment_size >
            SF64_TITLE_UI_VERTEX_OFFSET ||
        SF64_TITLE_UI_VERTEX_OFFSET +
                SF64_TITLE_UI_LOGO_CHUNK_COUNT * 4U *
                    SF64_TITLE_UI_VERTEX_BYTES >
            SF64_TITLE_UI_ORTHO_OFFSET ||
        SF64_TITLE_UI_IDENTITY_OFFSET + 64U > task->dram_size_bytes ||
        task->display_list_size_bytes < SF64_TITLE_UI_GFX_BYTES ||
        (task->display_list_size_bytes % SF64_TITLE_UI_GFX_BYTES) != 0U) {
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
        return -1;
    }

    memcpy(rdram + SF64_TITLE_UI_SEGMENT_OFFSET,
           title_segment, title_segment_size);
    make_ortho_matrix(ortho);
    make_identity_matrix(identity);
    matrix_write_n64(rdram + SF64_TITLE_UI_ORTHO_OFFSET, ortho);
    matrix_write_n64(rdram + SF64_TITLE_UI_IDENTITY_OFFSET, identity);

    original_command_count =
        task->display_list_size_bytes / SF64_TITLE_UI_GFX_BYTES;
    if (SF64_TITLE_UI_ROOT_OFFSET +
            (original_command_count - 1U + 6U +
             SF64_TITLE_UI_LOGO_CHUNK_COUNT * 11U + 1U) *
            SF64_TITLE_UI_GFX_BYTES >
        SF64_TITLE_UI_MATRIX_LIMIT) {
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

    append_command(rdram, &command_count,
                   SF64_TITLE_UI_GFX_ENDDL, 0U);
    task->display_list_size_bytes =
        command_count * SF64_TITLE_UI_GFX_BYTES;
    task->debug_name = "SF64 original title starfield, Arwing, and logo";

    if (info) {
        info->logo_strip_count = s_logo_rect_count;
        info->generated_command_count =
            command_count - original_command_count;
    }
    return 0;
}

#include "sf64_ps3/arwing.h"
#include "sf64_ps3/display.h"
#include "sf64_ps3/takeoff.h"

#include <math.h>
#include <string.h>

#define SF64_ARWING_SKELETON_OFFSET 0x00016610U
/* GSTATE_TITLE uses aAwWingsClosedAnim in Display_Arwing_Skel. */
#define SF64_ARWING_ANIMATION_OFFSET 0x00015c28U
#define SF64_ARWING_MAX_LIMBS 32U
#define SF64_ARWING_ROOT_LIST_OFFSET 0x00000100U
#define SF64_ARWING_MATRIX_OFFSET 0x00007000U
#define SF64_TITLE_TEAM_MATRIX_OFFSET 0x0008c000U
#define SF64_TITLE_TEAM_MATRIX_ADDRESS 0x8008c000U
#define SF64_TITLE_TEAM_MATRIX_LIMIT 128U
#define SF64_TITLE_TEAM_LIGHT_OFFSET 0x0008b000U
#define SF64_TITLE_TEAM_LIGHT_ADDRESS 0x8008b000U
#define SF64_TITLE_SEGMENT_RDRAM_OFFSET 0x00030000U
#define SF64_ARWING_LIGHT_OFFSET 0x00008000U
#define SF64_ARWING_PROJECTION_OFFSET 0x00008040U
#define SF64_ARWING_VIEW_MATRIX_OFFSET 0x00008080U
#define SF64_ARWING_SEGMENT_RDRAM_OFFSET 0x00010000U
#define SF64_ARWING_ROOT_LIST_ADDRESS 0x80000100U
#define SF64_ARWING_MATRIX_ADDRESS 0x80007000U
#define SF64_ARWING_LIGHT_ADDRESS 0x80008000U
#define SF64_ARWING_PROJECTION_ADDRESS 0x80008040U
#define SF64_ARWING_VIEW_MATRIX_ADDRESS 0x80008080U
#define SF64_ARWING_SEGMENT_ADDRESS 0x80010000U
#define SF64_ARWING_G_MTX_MODELVIEW_LOAD 0x01020040U
#define SF64_ARWING_G_MTX_PROJECTION_LOAD 0x01060040U
#define SF64_ARWING_G_DL 0x06000000U
#define SF64_ARWING_G_ENDDL 0xb8000000U
#define SF64_ARWING_G_MOVEWORD_SEGMENT 0xbc000c06U
#define SF64_ARWING_G_MOVEWORD_NUMLIGHT 0xbc000002U
#define SF64_ARWING_G_MOVEMEM_LIGHT 0x03860010U
#define SF64_ARWING_G_MOVEMEM_AMBIENT 0x03880010U
#define SF64_ARWING_G_TEXTURE 0xd7000001U
#define SF64_ARWING_G_SETGEOMETRYMODE 0xb7000000U
#define SF64_ARWING_G_RDP_PIPESYNC 0xe7000000U
#define SF64_ARWING_G_RDP_SETFILLCOLOR 0xf7000000U
#define SF64_ARWING_G_RDP_FILLRECT 0xf6000000U
#define SF64_ARWING_G_LIGHTING_ZBUFFER 0x00020001U
#define SF64_ARWING_G_MTX_BYTES 64U
#define SF64_ARWING_GFX_COMMAND_BYTES 8U
#define SF64_ARWING_DLIST_COMMANDS_PER_LIMB 2U
#define SF64_ARWING_LIGHT_DIRECTIONAL_BYTES 16U
#define SF64_ARWING_LIGHT_AMBIENT_BYTES 16U
#define SF64_TITLE_SCREEN_WIDTH 320U
#define SF64_TITLE_SCREEN_HEIGHT 240U
#define SF64_TITLE_STAR_COUNT 1000U
#define SF64_ARWING_TWO_PI 6.28318530717958647692f

typedef float Sf64ArwingMatrix[4][4];

typedef struct Sf64ArwingLimb {
    u32 display_list;
    float translation[3];
    u32 sibling;
    u32 child;
} Sf64ArwingLimb;

typedef struct Sf64ArwingBuilder {
    const u8 *segment;
    u32 segment_size;
    u32 segment_id;
    u32 matrix_offset;
    u32 matrix_address;
    const u32 *skeleton;
    float (*joint_angles)[3];
    u8 *rdram;
    u32 root_command_count;
    u32 display_list_count;
    u32 title_star_count;
    u32 visited[SF64_ARWING_MAX_LIMBS];
    u32 visited_count;
} Sf64ArwingBuilder;

static const u16 s_title_star_colors[16] = {
    0x108bU, 0x108bU, 0x1087U, 0x1089U,
    0x39ffU, 0x190dU, 0x108bU, 0x1089U,
    0x294bU, 0x18dfU, 0x294bU, 0x1085U,
    0x39ffU, 0x108bU, 0x18cdU, 0x108bU
};
static Sf64ArwingTitlePose s_title_arwing_pose = {
    { -80.0f, 220.0f, -360.0f },
    4.0f,
    { 35.0f, -90.0f, -15.0f }
};
static int s_title_pose_from_original_logic;

void sf64_arwing_set_title_pose(const Sf64ArwingTitlePose *pose)
{
    if (pose) {
        s_title_arwing_pose = *pose;
        s_title_pose_from_original_logic = 1;
    }
}

static u16 read_be16(const u8 *bytes)
{
    return (u16)(((u16)bytes[0] << 8) | bytes[1]);
}

static u32 read_be32(const u8 *bytes)
{
    return ((u32)bytes[0] << 24) |
           ((u32)bytes[1] << 16) |
           ((u32)bytes[2] << 8) |
           (u32)bytes[3];
}

int sf64_title_running_animation(const uint8_t *segment, uint32_t segment_size,
                                 uint32_t team, uint32_t frame,
                                 Sf64TitleTeamModel *model, float root_translation[3])
{
    static const u32 animation_offsets[4] = {0x2f8e0U, 0x305c0U, 0x31db8U, 0x31120U};
    static const u32 skeleton_offsets[4] = {0x2fbacU, 0x3088cU, 0x32084U, 0x313acU};
    u32 offset, key_address, data_address, keys, data, limb_count, frame_count, joint, axis;

    if (!segment || !model || !root_translation || team >= 4U) return -1;
    offset = animation_offsets[team];
    if (offset > segment_size || 12U > segment_size - offset) return -1;
    frame_count = read_be16(segment + offset);
    limb_count = read_be16(segment + offset + 2U);
    data_address = read_be32(segment + offset + 4U);
    key_address = read_be32(segment + offset + 8U);
    if (frame_count == 0 || limb_count == 0 || limb_count >= SF64_TITLE_TEAM_MAX_LIMBS ||
        (data_address >> 24) != 6U || (key_address >> 24) != 6U) return -1;
    frame %= frame_count;
    data = data_address & 0xffffffU;
    keys = key_address & 0xffffffU;
    if (data >= segment_size || keys > segment_size ||
        (limb_count + 1U) * 12U > segment_size - keys) return -1;
    model->skeleton_address = 0x06000000U | skeleton_offsets[team];
    model->limb_count = limb_count;
    memset(model->joint_rotation_degrees, 0, sizeof(model->joint_rotation_degrees));
    for (joint = 0; joint <= limb_count; joint++) {
        const u8 *key = segment + keys + joint * 12U;
        for (axis = 0; axis < 3U; axis++) {
            u32 length = read_be16(key + axis * 4U);
            u32 index = read_be16(key + axis * 4U + 2U);
            u32 position = data + (index + (frame < length ? frame : 0U)) * 2U;
            u16 sample;
            if (position > segment_size || 2U > segment_size - position) return -1;
            sample = read_be16(segment + position);
            if (joint == 0U) root_translation[axis] = (float)(s16)sample;
            else model->joint_rotation_degrees[joint - 1U][axis] = (float)sample * 360.0f / 65536.0f;
        }
    }
    return 0;
}

static void write_be16(u8 *bytes, u16 value)
{
    bytes[0] = (u8)(value >> 8);
    bytes[1] = (u8)value;
}

static void write_be32(u8 *bytes, u32 value)
{
    bytes[0] = (u8)(value >> 24);
    bytes[1] = (u8)(value >> 16);
    bytes[2] = (u8)(value >> 8);
    bytes[3] = (u8)value;
}

static float read_be_float(const u8 *bytes)
{
    u32 bits = read_be32(bytes);
    float value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int segment_offset(const Sf64ArwingBuilder *builder,
                          u32 address,
                          u32 required_size,
                          u32 *offset)
{
    u32 segment = address >> 24;
    u32 byte_offset = address & 0x00ffffffU;

    if (segment != builder->segment_id ||
        byte_offset > builder->segment_size ||
        required_size > builder->segment_size - byte_offset) {
        return -1;
    }
    *offset = byte_offset;
    return 0;
}

static void matrix_identity(Sf64ArwingMatrix matrix)
{
    u32 row;

    memset(matrix, 0, sizeof(Sf64ArwingMatrix));
    for (row = 0; row < 4U; row++) {
        matrix[row][row] = 1.0f;
    }
}

static void matrix_translate(Sf64ArwingMatrix matrix,
                             float x,
                             float y,
                             float z)
{
    u32 index;

    for (index = 0; index < 4U; index++) {
        matrix[3][index] += matrix[0][index] * x +
                            matrix[1][index] * y +
                            matrix[2][index] * z;
    }
}

static void matrix_scale(Sf64ArwingMatrix matrix,
                         float x_scale,
                         float y_scale,
                         float z_scale)
{
    u32 index;

    for (index = 0; index < 4U; index++) {
        matrix[0][index] *= x_scale;
        matrix[1][index] *= y_scale;
        matrix[2][index] *= z_scale;
    }
}

static void matrix_rotate_x(Sf64ArwingMatrix matrix, float angle)
{
    float cosine = cosf(angle);
    float sine = sinf(angle);
    u32 index;

    for (index = 0; index < 4U; index++) {
        float y = matrix[1][index];
        float z = matrix[2][index];

        matrix[1][index] = y * cosine + z * sine;
        matrix[2][index] = z * cosine - y * sine;
    }
}

static void matrix_rotate_y(Sf64ArwingMatrix matrix, float angle)
{
    float cosine = cosf(angle);
    float sine = sinf(angle);
    u32 index;

    for (index = 0; index < 4U; index++) {
        float x = matrix[0][index];
        float z = matrix[2][index];

        matrix[0][index] = x * cosine - z * sine;
        matrix[2][index] = x * sine + z * cosine;
    }
}

static void matrix_rotate_z(Sf64ArwingMatrix matrix, float angle)
{
    float cosine = cosf(angle);
    float sine = sinf(angle);
    u32 index;

    for (index = 0; index < 4U; index++) {
        float x = matrix[0][index];
        float y = matrix[1][index];

        matrix[0][index] = x * cosine + y * sine;
        matrix[1][index] = y * cosine - x * sine;
    }
}

static void matrix_look_at(Sf64ArwingMatrix matrix,
                           float eye_x,
                           float eye_y,
                           float eye_z,
                           float at_x,
                           float at_y,
                           float at_z,
                           float up_x,
                           float up_y,
                           float up_z)
{
    float look_x = at_x - eye_x;
    float look_y = at_y - eye_y;
    float look_z = at_z - eye_z;
    float length = sqrtf(look_x * look_x + look_y * look_y + look_z * look_z);
    float right_x;
    float right_y;
    float right_z;

    if (length == 0.0f) {
        matrix_identity(matrix);
        return;
    }

    /* Match libultra guLookAtF: positive Z points behind the camera. */
    look_x *= -1.0f / length;
    look_y *= -1.0f / length;
    look_z *= -1.0f / length;

    right_x = up_y * look_z - up_z * look_y;
    right_y = up_z * look_x - up_x * look_z;
    right_z = up_x * look_y - up_y * look_x;
    length = sqrtf(right_x * right_x + right_y * right_y + right_z * right_z);
    if (length == 0.0f) {
        matrix_identity(matrix);
        return;
    }
    right_x /= length;
    right_y /= length;
    right_z /= length;

    up_x = look_y * right_z - look_z * right_y;
    up_y = look_z * right_x - look_x * right_z;
    up_z = look_x * right_y - look_y * right_x;
    length = sqrtf(up_x * up_x + up_y * up_y + up_z * up_z);
    if (length == 0.0f) {
        matrix_identity(matrix);
        return;
    }
    up_x /= length;
    up_y /= length;
    up_z /= length;

    matrix_identity(matrix);
    matrix[0][0] = right_x;
    matrix[1][0] = right_y;
    matrix[2][0] = right_z;
    matrix[3][0] = -(eye_x * right_x + eye_y * right_y + eye_z * right_z);
    matrix[0][1] = up_x;
    matrix[1][1] = up_y;
    matrix[2][1] = up_z;
    matrix[3][1] = -(eye_x * up_x + eye_y * up_y + eye_z * up_z);
    matrix[0][2] = look_x;
    matrix[1][2] = look_y;
    matrix[2][2] = look_z;
    matrix[3][2] = -(eye_x * look_x + eye_y * look_y + eye_z * look_z);
}

static void matrix_perspective(Sf64ArwingMatrix matrix, float near_plane)
{
    const float degrees_to_radians = SF64_ARWING_TWO_PI / 360.0f;
    const float field_of_view_y = 45.0f;
    const float aspect = SF64_DISPLAY_REFERENCE_ASPECT;
    const float far_plane = 12800.0f;
    float half_fov = field_of_view_y * degrees_to_radians * 0.5f;
    float cotangent = cosf(half_fov) / sinf(half_fov);
    float scale_x;
    float scale_y;

    /* Match Starship's projected-X correction and PSP's display-aspect
     * perspective while keeping the original title's 320x240 world layout. */
    sf64_display_fit_4_3(sf64_display_aspect(), &scale_x, &scale_y);

    memset(matrix, 0, sizeof(Sf64ArwingMatrix));
    matrix[0][0] = (cotangent / aspect) * scale_x;
    matrix[1][1] = cotangent * scale_y;
    matrix[2][2] = (near_plane + far_plane) / (near_plane - far_plane);
    matrix[2][3] = -1.0f;
    matrix[3][2] = (2.0f * near_plane * far_plane) / (near_plane - far_plane);
}

static void matrix_transform_direction(Sf64ArwingMatrix matrix, const float input[3], float output[3]);

void sf64_camera_matrices(const float eye[3], const float at[3], const float up[3],
                          float projection[16], float view[16])
{
    Sf64ArwingMatrix p;
    Sf64ArwingMatrix v;

    matrix_perspective(p, 1.0f);
    matrix_look_at(v, eye[0], eye[1], eye[2], at[0], at[1], at[2], up[0], up[1], up[2]);
    memcpy(projection, p, sizeof(p));
    memcpy(view, v, sizeof(v));
}

void sf64_great_fox_matrices(const float eye[3], const float at[3], const float up[3],
                             const float position[3], float light_x, float light_y, float projection[16],
                             float modelview[16], float direction[3])
{
    const float radians = SF64_ARWING_TWO_PI / 360.0f;
    const float source[3] = {0.0f, 0.0f, 100.0f};
    float x = eye[0] - at[0], y = eye[1] - at[1], z = eye[2] - at[2];
    float camera_x = -atan2f(y, sqrtf(x * x + z * z));
    float camera_y = atan2f(x, z);
    Sf64ArwingMatrix p, v, light;
    matrix_perspective(p, 1.0f);
    matrix_look_at(v, eye[0], eye[1], eye[2], at[0], at[1], at[2], up[0], up[1], up[2]);
    matrix_translate(v, position[0], position[1], position[2]);
    matrix_scale(v, 0.1f, 0.1f, 0.1f);
    memcpy(projection, p, sizeof(p));
    memcpy(modelview, v, sizeof(v));
    matrix_identity(light);
    matrix_rotate_x(light, -camera_x);
    matrix_rotate_y(light, -camera_y);
    matrix_rotate_y(light, light_y * radians);
    matrix_rotate_x(light, light_x * radians);
    matrix_transform_direction(light, source, direction);
}

static void matrix_transform_direction(Sf64ArwingMatrix matrix,
                                       const float input[3],
                                       float output[3])
{
    output[0] = matrix[0][0] * input[0] +
                matrix[1][0] * input[1] +
                matrix[2][0] * input[2];
    output[1] = matrix[0][1] * input[0] +
                matrix[1][1] * input[1] +
                matrix[2][1] * input[2];
    output[2] = matrix[0][2] * input[0] +
                matrix[1][2] * input[1] +
                matrix[2][2] * input[2];
}

static void matrix_multiply(Sf64ArwingMatrix output,
                            Sf64ArwingMatrix left,
                            Sf64ArwingMatrix right)
{
    Sf64ArwingMatrix result;
    u32 row;
    u32 column;
    u32 inner;

    for (row = 0; row < 4U; row++) {
        for (column = 0; column < 4U; column++) {
            float value = 0.0f;
            for (inner = 0; inner < 4U; inner++) {
                value += left[inner][column] * right[row][inner];
            }
            result[row][column] = value;
        }
    }
    memcpy(output, result, sizeof(result));
}

static void matrix_write_n64(u8 *destination,
                             Sf64ArwingMatrix matrix)
{
    u32 row;
    u32 column;

    for (row = 0; row < 4U; row++) {
        for (column = 0; column < 4U; column++) {
            s32 fixed = (s32)(matrix[row][column] * 65536.0f);
            u32 encoded = (u32)fixed;
            u32 index = row * 4U + column;

            write_be16(destination + index * 2U,
                       (u16)(encoded >> 16));
            write_be16(destination + 32U + index * 2U,
                       (u16)encoded);
        }
    }
}

static void list_append(Sf64ArwingBuilder *builder, u32 word0, u32 word1)
{
    u8 *command = builder->rdram + SF64_ARWING_ROOT_LIST_OFFSET +
                  builder->root_command_count * SF64_ARWING_GFX_COMMAND_BYTES;

    write_be32(command, word0);
    write_be32(command + 4U, word1);
    builder->root_command_count++;
}

static float title_star_random(u32 *seed1, u32 *seed2, u32 *seed3)
{
    float value;

    *seed1 = (*seed1 * 171U) % 30269U;
    *seed2 = (*seed2 * 172U) % 30307U;
    *seed3 = (*seed3 * 170U) % 30323U;
    value = (float)*seed1 / 30269.0f +
            (float)*seed2 / 30307.0f +
            (float)*seed3 / 30323.0f;
    return value - (s32)value;
}

static void append_title_starfield(Sf64ArwingBuilder *builder)
{
    u32 seed1 = 1U;
    u32 seed2 = 29000U;
    u32 seed3 = 9876U;
    u32 index;

    for (index = 0; index < SF64_TITLE_STAR_COUNT; index++) {
        float x_star = title_star_random(&seed1, &seed2, &seed3) * 480.0f - 80.0f;
        float y_star = title_star_random(&seed1, &seed2, &seed3) * 360.0f - 60.0f;
        float bx = x_star + (float)SF64_TITLE_SCREEN_WIDTH;
        float by = y_star + (float)SF64_TITLE_SCREEN_HEIGHT;
        s16 vx;
        s16 vy;

        if (bx >= 1.25f * (float)SF64_TITLE_SCREEN_WIDTH) {
            bx -= 1.5f * (float)SF64_TITLE_SCREEN_WIDTH;
        }
        bx -= (float)SF64_TITLE_SCREEN_WIDTH / 2.0f;
        if (by >= 1.25f * (float)SF64_TITLE_SCREEN_HEIGHT) {
            by -= 1.5f * (float)SF64_TITLE_SCREEN_HEIGHT;
        }
        by -= (float)SF64_TITLE_SCREEN_HEIGHT / 2.0f;
        vx = (s16)(bx + (float)SF64_TITLE_SCREEN_WIDTH / 2.0f);
        vy = (s16)(by + (float)SF64_TITLE_SCREEN_HEIGHT / 2.0f);

        if (vx >= 0 && vx < (s16)SF64_TITLE_SCREEN_WIDTH &&
            vy > 0 && vy < (s16)SF64_TITLE_SCREEN_HEIGHT) {
            u32 color = s_title_star_colors[index & 15U];
            u32 x_pixel = (u32)(u16)vx;
            u32 y_pixel = (u32)(u16)vy;

            list_append(builder, SF64_ARWING_G_RDP_PIPESYNC, 0U);
            list_append(builder, SF64_ARWING_G_RDP_SETFILLCOLOR,
                        (color << 16) | color);
            list_append(builder,
                        SF64_ARWING_G_RDP_FILLRECT | (x_pixel << 14) |
                            (y_pixel << 2),
                        (x_pixel << 14) | (y_pixel << 2));
            builder->title_star_count++;
        }
    }
    list_append(builder, SF64_ARWING_G_RDP_PIPESYNC, 0U);
}

static int find_joint_index(const Sf64ArwingBuilder *builder,
                            u32 limb_address,
                            u32 *joint_index)
{
    u32 index;

    for (index = 0; index < SF64_ARWING_MAX_LIMBS; index++) {
        if (builder->skeleton[index] == 0U) {
            break;
        }
        if (builder->skeleton[index] == limb_address) {
            *joint_index = index + 1U;
            return 0;
        }
    }
    return -1;
}

static int limb_read(const Sf64ArwingBuilder *builder,
                     u32 address,
                     Sf64ArwingLimb *limb)
{
    u32 offset;

    if (segment_offset(builder, address, 0x20U, &offset) != 0) {
        return -1;
    }
    limb->display_list = read_be32(builder->segment + offset);
    limb->translation[0] = read_be_float(builder->segment + offset + 4U);
    limb->translation[1] = read_be_float(builder->segment + offset + 8U);
    limb->translation[2] = read_be_float(builder->segment + offset + 12U);
    limb->sibling = read_be32(builder->segment + offset + 24U);
    limb->child = read_be32(builder->segment + offset + 28U);
    return 0;
}

static int limb_was_visited(const Sf64ArwingBuilder *builder, u32 address)
{
    u32 index;

    for (index = 0; index < builder->visited_count; index++) {
        if (builder->visited[index] == address) {
            return 1;
        }
    }
    return 0;
}

static int emit_limb(Sf64ArwingBuilder *builder,
                     u32 address,
                     Sf64ArwingMatrix *parent_matrix,
                     Sf64ArwingMatrix *world_matrix)
{
    Sf64ArwingLimb limb;
    Sf64ArwingMatrix local_matrix;
    Sf64ArwingMatrix final_matrix;
    u32 joint_index;

    if (address == 0U) {
        return 0;
    }
    if (builder->visited_count >= SF64_ARWING_MAX_LIMBS ||
        limb_was_visited(builder, address) ||
        find_joint_index(builder, address, &joint_index) != 0 ||
        limb_read(builder, address, &limb) != 0) {
        return -1;
    }
    builder->visited[builder->visited_count++] = address;

    memcpy(local_matrix, *parent_matrix, sizeof(local_matrix));
    matrix_translate(local_matrix,
                     limb.translation[0],
                     limb.translation[1],
                     limb.translation[2]);
    matrix_rotate_z(local_matrix, builder->joint_angles[joint_index][2]);
    matrix_rotate_y(local_matrix, builder->joint_angles[joint_index][1]);
    matrix_rotate_x(local_matrix, builder->joint_angles[joint_index][0]);

    if (limb.display_list != 0U) {
        u32 matrix_offset = builder->matrix_offset +
                            builder->display_list_count * SF64_ARWING_G_MTX_BYTES;
        u8 *matrix_bytes;

        if (builder->display_list_count >= SF64_TITLE_TEAM_MATRIX_LIMIT ||
            segment_offset(builder, limb.display_list, 8U, &joint_index) != 0) {
            return -1;
        }
        matrix_multiply(final_matrix, *world_matrix, local_matrix);
        matrix_bytes = builder->rdram + matrix_offset;
        matrix_write_n64(matrix_bytes, final_matrix);
        list_append(builder,
                    SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                    builder->matrix_address +
                        builder->display_list_count * SF64_ARWING_G_MTX_BYTES);
        list_append(builder, SF64_ARWING_G_DL, limb.display_list);
        builder->display_list_count++;
    }

    if (limb.child != 0U &&
        emit_limb(builder, limb.child, &local_matrix, world_matrix) != 0) {
        return -1;
    }
    if (limb.sibling != 0U &&
        emit_limb(builder, limb.sibling, parent_matrix, world_matrix) != 0) {
        return -1;
    }
    return 0;
}

static int animation_read(const u8 *segment,
                          u32 segment_size,
                          u32 *limb_count,
                          float joint_angles[SF64_ARWING_MAX_LIMBS + 1U][3])
{
    s16 animation_limb_count;
    u32 frame_data_address;
    u32 joint_key_address;
    u32 frame_data_offset;
    u32 joint_key_offset;
    u32 index;

    if (SF64_ARWING_ANIMATION_OFFSET + 12U > segment_size) {
        return -1;
    }
    animation_limb_count = (s16)read_be16(segment + SF64_ARWING_ANIMATION_OFFSET + 2U);
    frame_data_address = read_be32(segment + SF64_ARWING_ANIMATION_OFFSET + 4U);
    joint_key_address = read_be32(segment + SF64_ARWING_ANIMATION_OFFSET + 8U);
    if (animation_limb_count <= 0 ||
        (u32)animation_limb_count > SF64_ARWING_MAX_LIMBS - 1U ||
        (frame_data_address >> 24) != SF64_ARWING_SEGMENT_ID ||
        (joint_key_address >> 24) != SF64_ARWING_SEGMENT_ID) {
        return -1;
    }
    frame_data_offset = frame_data_address & 0x00ffffffU;
    joint_key_offset = joint_key_address & 0x00ffffffU;
    if (frame_data_offset >= segment_size ||
        joint_key_offset > segment_size ||
        ((u32)animation_limb_count + 1U) * 12U > segment_size - joint_key_offset) {
        return -1;
    }

    memset(joint_angles, 0,
           sizeof(float) * (SF64_ARWING_MAX_LIMBS + 1U) * 3U);
    for (index = 0; index <= (u32)animation_limb_count; index++) {
        const u8 *key = segment + joint_key_offset + index * 12U;
        u16 x_offset = read_be16(key + 2U);
        u16 y_offset = read_be16(key + 6U);
        u16 z_offset = read_be16(key + 10U);
        u32 frame_offsets[3];
        u32 axis;

        frame_offsets[0] = x_offset;
        frame_offsets[1] = y_offset;
        frame_offsets[2] = z_offset;
        for (axis = 0; axis < 3U; axis++) {
            u32 data_offset = frame_data_offset + frame_offsets[axis] * 2U;
            s16 frame_value;

            if (data_offset > segment_size || 2U > segment_size - data_offset) {
                return -1;
            }
            frame_value = (s16)read_be16(segment + data_offset);
            if (index != 0U) {
                joint_angles[index][axis] =
                    ((float)frame_value * SF64_ARWING_TWO_PI) / 65536.0f;
            }
        }
    }
    *limb_count = (u32)animation_limb_count;
    return 0;
}

static int skeleton_read(const u8 *segment,
                         u32 segment_size,
                         u32 segment_id,
                         u32 skeleton[SF64_ARWING_MAX_LIMBS],
                         u32 *limb_count)
{
    u32 index;

    memset(skeleton, 0, sizeof(u32) * SF64_ARWING_MAX_LIMBS);
    if (SF64_ARWING_SKELETON_OFFSET > segment_size) {
        return -1;
    }
    for (index = 0; index < SF64_ARWING_MAX_LIMBS; index++) {
        u32 pointer_offset = SF64_ARWING_SKELETON_OFFSET + index * 4U;
        u32 address;
        if (pointer_offset > segment_size || 4U > segment_size - pointer_offset) {
            return -1;
        }
        address = read_be32(segment + pointer_offset);
        if (address == 0U) {
            *limb_count = index;
            return index != 0U ? 0 : -1;
        }
        if ((address >> 24) != segment_id ||
            (address & 0x00ffffffU) > segment_size ||
            0x20U > segment_size - (address & 0x00ffffffU)) {
            return -1;
        }
        skeleton[index] = address;
    }
    return -1;
}

static void build_title_world_matrix(Sf64ArwingMatrix matrix,
                                     Sf64ArwingMatrix view_matrix,
                                     const Sf64ArwingTitlePose *pose)
{
    const float degrees_to_radians = SF64_ARWING_TWO_PI / 360.0f;

    memcpy(matrix, view_matrix, sizeof(Sf64ArwingMatrix));
    matrix_translate(matrix,
                     pose->position[0],
                     pose->position[1],
                     pose->position[2]);
    matrix_scale(matrix, pose->scale, pose->scale, pose->scale);
    matrix_rotate_z(matrix, pose->rotation_degrees[2] * degrees_to_radians);
    matrix_rotate_x(matrix, pose->rotation_degrees[0] * degrees_to_radians);
    matrix_rotate_y(matrix, pose->rotation_degrees[1] * degrees_to_radians);
}

static void write_light_data(u8 *rdram)
{
    const float degrees_to_radians = SF64_ARWING_TWO_PI / 360.0f;
    const float eye_x = 0.0f;
    const float eye_y = 180.0f;
    const float eye_z = 430.0f;
    const float at_x = 0.0f;
    const float at_y = 180.0f;
    const float at_z = 0.0f;
    const float source_direction[3] = { 0.0f, 0.0f, 100.0f };
    u8 *directional = rdram + SF64_ARWING_LIGHT_OFFSET;
    u8 *ambient = directional + SF64_ARWING_LIGHT_DIRECTIONAL_BYTES;
    Sf64ArwingMatrix light_matrix;
    float direction[3];
    float x = eye_x - at_x;
    float y = eye_y - at_y;
    float z = eye_z - at_z;
    float x_rotation = -atan2f(y, sqrtf(x * x + z * z)) /
                       degrees_to_radians;
    float y_rotation = atan2f(x, z) / degrees_to_radians;

    memset(directional, 0, SF64_ARWING_LIGHT_DIRECTIONAL_BYTES);
    directional[0] = 30U;
    directional[1] = 29U;
    directional[2] = 40U;
    matrix_identity(light_matrix);
    matrix_rotate_x(light_matrix, -x_rotation * degrees_to_radians);
    matrix_rotate_y(light_matrix, -y_rotation * degrees_to_radians);
    matrix_rotate_y(light_matrix, 22.0f * degrees_to_radians);
    matrix_rotate_x(light_matrix, -70.0f * degrees_to_radians);
    matrix_transform_direction(light_matrix, source_direction, direction);
    directional[8] = (u8)((s32)direction[0] & 0xff);
    directional[9] = (u8)((s32)direction[1] & 0xff);
    directional[10] = (u8)((s32)direction[2] & 0xff);

    memset(ambient, 0, SF64_ARWING_LIGHT_AMBIENT_BYTES);
    ambient[0] = 6U;
    ambient[1] = 1U;
    ambient[2] = 11U;
}

static void write_title_team_light_data(
    u8 *rdram, const Sf64OriginalTitleLighting *lighting)
{
    u8 *directional = rdram + SF64_TITLE_TEAM_LIGHT_OFFSET;
    u8 *ambient = directional + SF64_ARWING_LIGHT_DIRECTIONAL_BYTES;
    u32 axis;

    memset(directional, 0, SF64_ARWING_LIGHT_DIRECTIONAL_BYTES);
    for (axis = 0U; axis < 3U; axis++) {
        directional[axis] = lighting->directional_rgb[axis];
        directional[8U + axis] = (u8)((s32)lighting->direction[axis] & 0xff);
    }
    memset(ambient, 0, SF64_ARWING_LIGHT_AMBIENT_BYTES);
    for (axis = 0U; axis < 3U; axis++) {
        ambient[axis] = lighting->ambient_rgb[axis];
    }
}

static int sf64_arwing_build_title_task_internal(
    const u8 *segment,
    u32 segment_size,
    const u8 *starfield_display_list,
    u32 starfield_display_list_size,
    u32 title_star_count,
    u8 *rdram,
    u32 rdram_size,
    N64ps3GfxTask *task,
    Sf64ArwingBuildInfo *info)
{
    u32 skeleton[SF64_ARWING_MAX_LIMBS];
    float joint_angles[SF64_ARWING_MAX_LIMBS + 1U][3];
    u32 skeleton_limb_count;
    u32 animation_limb_count;
    Sf64ArwingBuilder builder;
    Sf64ArwingMatrix projection_matrix;
    Sf64ArwingMatrix view_matrix;
    Sf64ArwingMatrix world_matrix;
    Sf64ArwingMatrix root_matrix;
    Sf64ArwingLimb root_limb;
    u32 root_joint_index;
    u32 command_bytes;

    if (!segment || !rdram || !task ||
        segment_size < SF64_ARWING_SEGMENT_SIZE ||
        rdram_size < SF64_ARWING_RDRAM_REQUIRED_SIZE) {
        return -1;
    }
    if (starfield_display_list &&
        (starfield_display_list_size < SF64_ARWING_GFX_COMMAND_BYTES ||
         (starfield_display_list_size % SF64_ARWING_GFX_COMMAND_BYTES) != 0U ||
         starfield_display_list_size > SF64_TITLE_STARFIELD_RDRAM_SIZE ||
         title_star_count > SF64_TITLE_STAR_COUNT)) {
        return -1;
    }
    memset(rdram, 0, rdram_size);
    memcpy(rdram + SF64_ARWING_SEGMENT_RDRAM_OFFSET,
           segment, SF64_ARWING_SEGMENT_SIZE);
    if (starfield_display_list) {
        memcpy(rdram + SF64_TITLE_STARFIELD_RDRAM_OFFSET,
               starfield_display_list, starfield_display_list_size);
    }

    if (skeleton_read(segment, segment_size, SF64_ARWING_SEGMENT_ID,
                      skeleton, &skeleton_limb_count) != 0 ||
        animation_read(segment, segment_size, &animation_limb_count, joint_angles) != 0 ||
        skeleton_limb_count != animation_limb_count ||
        limb_read(&(Sf64ArwingBuilder){ .segment = segment,
                                        .segment_size = segment_size,
                                        .segment_id = SF64_ARWING_SEGMENT_ID },
                  skeleton[0], &root_limb) != 0 ||
        find_joint_index(&(Sf64ArwingBuilder){ .skeleton = skeleton },
                         skeleton[0], &root_joint_index) != 0) {
        return -1;
    }

    matrix_look_at(view_matrix,
                   0.0f, 180.0f, 430.0f,
                   0.0f, 180.0f, 0.0f,
                   0.0f, 1.0f, 0.0f);
    matrix_perspective(projection_matrix, 1.0f);
    matrix_write_n64(rdram + SF64_ARWING_PROJECTION_OFFSET, projection_matrix);
    matrix_write_n64(rdram + SF64_ARWING_VIEW_MATRIX_OFFSET, view_matrix);
    write_light_data(rdram);
    memset(&builder, 0, sizeof(builder));
    builder.segment = segment;
    builder.segment_size = segment_size;
    builder.segment_id = SF64_ARWING_SEGMENT_ID;
    builder.matrix_offset = SF64_ARWING_MATRIX_OFFSET;
    builder.matrix_address = SF64_ARWING_MATRIX_ADDRESS;
    builder.skeleton = skeleton;
    builder.joint_angles = joint_angles;
    builder.rdram = rdram;

    matrix_identity(root_matrix);
    matrix_translate(root_matrix,
                     root_limb.translation[0],
                     root_limb.translation[1],
                     root_limb.translation[2]);
    matrix_rotate_z(root_matrix, joint_angles[root_joint_index][2]);
    matrix_rotate_y(root_matrix, joint_angles[root_joint_index][1]);
    matrix_rotate_x(root_matrix, joint_angles[root_joint_index][0]);
    if (!s_title_pose_from_original_logic) {
        s_title_arwing_pose.rotation_degrees[0] += 0.6f;
    }
    build_title_world_matrix(world_matrix, view_matrix,
                             &s_title_arwing_pose);

    list_append(&builder, SF64_ARWING_G_MOVEWORD_SEGMENT,
                SF64_ARWING_SEGMENT_ADDRESS);
    if (starfield_display_list) {
        list_append(&builder, 0xbc001006U, 0x8002a000U);
        list_append(&builder, SF64_ARWING_G_DL, 0x04000000U);
        builder.title_star_count = title_star_count;
    } else {
        append_title_starfield(&builder);
    }
    list_append(&builder, SF64_ARWING_G_MTX_PROJECTION_LOAD,
                SF64_ARWING_PROJECTION_ADDRESS);
    list_append(&builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                SF64_ARWING_VIEW_MATRIX_ADDRESS);
    list_append(&builder, SF64_ARWING_G_MOVEWORD_NUMLIGHT, 0x00000040U);
    list_append(&builder, SF64_ARWING_G_MOVEMEM_LIGHT,
                SF64_ARWING_LIGHT_ADDRESS);
    list_append(&builder, SF64_ARWING_G_MOVEMEM_AMBIENT,
                SF64_ARWING_LIGHT_ADDRESS + SF64_ARWING_LIGHT_DIRECTIONAL_BYTES);
    list_append(&builder, SF64_ARWING_G_TEXTURE, 0xffffffffU);
    list_append(&builder, SF64_ARWING_G_SETGEOMETRYMODE,
                SF64_ARWING_G_LIGHTING_ZBUFFER);

    if (root_limb.display_list != 0U) {
        Sf64ArwingMatrix final_matrix;
        u32 matrix_address = SF64_ARWING_MATRIX_ADDRESS;

        matrix_multiply(final_matrix, world_matrix, root_matrix);
        matrix_write_n64(rdram + SF64_ARWING_MATRIX_OFFSET, final_matrix);
        list_append(&builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD, matrix_address);
        list_append(&builder, SF64_ARWING_G_DL, root_limb.display_list);
        builder.display_list_count++;
    }
    if (root_limb.child != 0U &&
        emit_limb(&builder, root_limb.child, &root_matrix, &world_matrix) != 0) {
        return -1;
    }
    list_append(&builder, SF64_ARWING_G_ENDDL, 0U);
    command_bytes = builder.root_command_count * SF64_ARWING_GFX_COMMAND_BYTES;
    if (SF64_ARWING_ROOT_LIST_OFFSET + command_bytes > SF64_ARWING_MATRIX_OFFSET ||
        SF64_ARWING_MATRIX_OFFSET + builder.display_list_count * SF64_ARWING_G_MTX_BYTES >
            SF64_ARWING_LIGHT_OFFSET ||
        SF64_ARWING_LIGHT_OFFSET + SF64_ARWING_LIGHT_DIRECTIONAL_BYTES +
            SF64_ARWING_LIGHT_AMBIENT_BYTES > SF64_ARWING_SEGMENT_RDRAM_OFFSET) {
        return -1;
    }

    memset(task, 0, sizeof(*task));
    task->kind = N64PS3_GFX_TASK_DISPLAY_LIST;
    task->dram_base = rdram;
    task->dram_size_bytes = rdram_size;
    task->display_list = rdram + SF64_ARWING_ROOT_LIST_OFFSET;
    task->display_list_size_bytes = command_bytes;
    task->debug_name = starfield_display_list
                           ? "SF64 original title starfield and Arwing"
                           : "SF64 title starfield and Arwing probe";

    if (info) {
        info->animation_frame = 0U;
        info->skeleton_limb_count = skeleton_limb_count;
        info->display_list_count = builder.display_list_count;
        info->title_star_count = builder.title_star_count;
        info->generated_command_count = builder.root_command_count;
    }
    return 0;
}

int sf64_arwing_build_title_task(const u8 *segment,
                                 u32 segment_size,
                                 u8 *rdram,
                                 u32 rdram_size,
                                 N64ps3GfxTask *task,
                                 Sf64ArwingBuildInfo *info)
{
    return sf64_arwing_build_title_task_internal(segment, segment_size,
                                                 NULL, 0U, 0U,
                                                 rdram, rdram_size,
                                                 task, info);
}

int sf64_arwing_build_title_task_with_starfield(
    const u8 *segment,
    u32 segment_size,
    const u8 *starfield_display_list,
    u32 starfield_display_list_size,
    u32 title_star_count,
    u8 *rdram,
    u32 rdram_size,
    N64ps3GfxTask *task,
    Sf64ArwingBuildInfo *info)
{
    if (!starfield_display_list) {
        return -1;
    }
    return sf64_arwing_build_title_task_internal(
        segment, segment_size, starfield_display_list,
        starfield_display_list_size, title_star_count,
        rdram, rdram_size, task, info);
}

static int append_title_teams_view(N64ps3GfxTask *task,
                                   const u8 *title_segment,
                                   u32 title_segment_size,
                                   const Sf64TitleTeamModel models[4],
                                   const Sf64OriginalTitleLighting *lighting,
                                   u32 *limb_count,
                                   u32 *display_list_count,
                                   const float *eye, const float *at, const float *up)
{
    u8 *rdram;
    u32 total_limbs = 0U;
    u32 current_display_lists;
    u32 command_count;
    u32 team_index;
    Sf64ArwingMatrix projection_matrix;
    Sf64ArwingMatrix view_matrix;
    const float radians_per_degree = SF64_ARWING_TWO_PI / 360.0f;

    if (!task || !task->dram_base || !task->display_list ||
        !title_segment || !models || !lighting ||
        task->kind != N64PS3_GFX_TASK_DISPLAY_LIST ||
        SF64_TITLE_SEGMENT_RDRAM_OFFSET + title_segment_size >
            task->dram_size_bytes ||
        task->display_list_size_bytes < SF64_ARWING_GFX_COMMAND_BYTES ||
        (task->display_list_size_bytes % SF64_ARWING_GFX_COMMAND_BYTES) != 0U) {
        return -1;
    }

    rdram = (u8 *)(uintptr_t)task->dram_base;
    memcpy(rdram + SF64_TITLE_SEGMENT_RDRAM_OFFSET,
           title_segment, title_segment_size);
    current_display_lists = 0U;
    command_count = (u32)(task->display_list_size_bytes /
                          SF64_ARWING_GFX_COMMAND_BYTES) - 1U;
    if (SF64_ARWING_ROOT_LIST_OFFSET +
            (command_count + 512U) * SF64_ARWING_GFX_COMMAND_BYTES >
        SF64_ARWING_MATRIX_OFFSET) {
        return -1;
    }

    if (eye && at && up) {
        matrix_look_at(view_matrix, eye[0], eye[1], eye[2], at[0], at[1], at[2], up[0], up[1], up[2]);
    } else {
        matrix_look_at(view_matrix,
                   0.0f, 180.0f, 430.0f,
                   0.0f, 180.0f, 0.0f,
                   0.0f, 1.0f, 0.0f);
    }
    matrix_perspective(projection_matrix, 1.0f);
    matrix_write_n64(rdram + SF64_ARWING_PROJECTION_OFFSET,
                     projection_matrix);
    matrix_write_n64(rdram + SF64_ARWING_VIEW_MATRIX_OFFSET, view_matrix);
    write_title_team_light_data(rdram, lighting);
    list_append(&(Sf64ArwingBuilder){
                    .rdram = rdram,
                    .root_command_count = command_count,
                },
                0xbc001806U, 0x80030000U);
    command_count++;
    list_append(&(Sf64ArwingBuilder){
                    .rdram = rdram,
                    .root_command_count = command_count,
                },
                SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                SF64_ARWING_VIEW_MATRIX_ADDRESS);
    command_count++;
    list_append(&(Sf64ArwingBuilder){
                    .rdram = rdram,
                    .root_command_count = command_count,
                },
                SF64_ARWING_G_MOVEWORD_NUMLIGHT, 0x00000040U);
    command_count++;
    list_append(&(Sf64ArwingBuilder){
                    .rdram = rdram,
                    .root_command_count = command_count,
                },
                SF64_ARWING_G_MOVEMEM_LIGHT,
                SF64_TITLE_TEAM_LIGHT_ADDRESS);
    command_count++;
    list_append(&(Sf64ArwingBuilder){
                    .rdram = rdram,
                    .root_command_count = command_count,
                },
                SF64_ARWING_G_MOVEMEM_AMBIENT,
                SF64_TITLE_TEAM_LIGHT_ADDRESS +
                    SF64_ARWING_LIGHT_DIRECTIONAL_BYTES);
    command_count++;
    list_append(&(Sf64ArwingBuilder){
                    .rdram = rdram,
                    .root_command_count = command_count,
                },
                SF64_ARWING_G_SETGEOMETRYMODE,
                SF64_ARWING_G_LIGHTING_ZBUFFER);
    command_count++;

    for (team_index = 0U; team_index < 4U; team_index++) {
        u32 skeleton[SF64_ARWING_MAX_LIMBS];
        u32 skeleton_limb_count;
        u32 root_joint_index;
        Sf64ArwingBuilder builder;
        Sf64ArwingLimb root_limb;
        float joint_angles[SF64_ARWING_MAX_LIMBS + 1U][3];
        Sf64ArwingMatrix parent_matrix;
        Sf64ArwingMatrix world_matrix;
        Sf64ArwingMatrix title_world_matrix;
        Sf64ArwingMatrix final_matrix;
        u32 skeleton_offset = models[team_index].skeleton_address & 0x00ffffffU;
        u32 index;

        if ((models[team_index].skeleton_address >> 24) != 6U) {
            return -1;
        }
        /* The team skeleton pointer is a segmented pointer to a pointer table. */
        for (index = 0U; index < SF64_ARWING_MAX_LIMBS; index++) {
            u32 pointer_offset = skeleton_offset + index * 4U;
            u32 address;
            if (pointer_offset > title_segment_size ||
                4U > title_segment_size - pointer_offset) {
                return -1;
            }
            address = read_be32(title_segment + pointer_offset);
            if (address == 0U) {
                skeleton_limb_count = index;
                skeleton[index] = 0U;
                break;
            }
            if ((address >> 24) != 6U ||
                (address & 0x00ffffffU) > title_segment_size ||
                0x20U > title_segment_size - (address & 0x00ffffffU)) {
                return -1;
            }
            skeleton[index] = address;
        }
        if (index == SF64_ARWING_MAX_LIMBS ||
            skeleton_limb_count != models[team_index].limb_count ||
            find_joint_index(&(Sf64ArwingBuilder){ .skeleton = skeleton },
                             skeleton[0], &root_joint_index) != 0) {
            return -1;
        }

        memset(&builder, 0, sizeof(builder));
        builder.segment = title_segment;
        builder.segment_size = title_segment_size;
        builder.segment_id = 6U;
        builder.matrix_offset = SF64_TITLE_TEAM_MATRIX_OFFSET;
        builder.matrix_address = SF64_TITLE_TEAM_MATRIX_ADDRESS;
        builder.skeleton = skeleton;
        builder.joint_angles = joint_angles;
        builder.rdram = rdram;
        builder.root_command_count = command_count;
        builder.display_list_count = current_display_lists;
        for (index = 0U; index < models[team_index].limb_count; index++) {
            builder.joint_angles[index + 1U][0] =
                models[team_index].joint_rotation_degrees[index][0] *
                radians_per_degree;
            builder.joint_angles[index + 1U][1] =
                models[team_index].joint_rotation_degrees[index][1] *
                radians_per_degree;
            builder.joint_angles[index + 1U][2] =
                models[team_index].joint_rotation_degrees[index][2] *
                radians_per_degree;
        }
        if (limb_read(&(Sf64ArwingBuilder){
                           .segment = title_segment,
                           .segment_size = title_segment_size,
                           .segment_id = 6U,
                       },
                       skeleton[0], &root_limb) != 0) {
            return -1;
        }

        matrix_identity(title_world_matrix);
        matrix_translate(title_world_matrix,
                         models[team_index].position[0],
                         models[team_index].position[1],
                         models[team_index].position[2]);
        matrix_scale(title_world_matrix, models[team_index].scale,
                     models[team_index].scale, models[team_index].scale);
        matrix_rotate_z(title_world_matrix,
                        models[team_index].rotation_degrees[2] *
                            radians_per_degree);
        matrix_rotate_y(title_world_matrix,
                        models[team_index].rotation_degrees[1] *
                            radians_per_degree);
        matrix_rotate_x(title_world_matrix,
                        models[team_index].rotation_degrees[0] *
                            radians_per_degree);
        matrix_multiply(world_matrix, view_matrix, title_world_matrix);

        matrix_identity(parent_matrix);
        matrix_translate(parent_matrix,
                         models[team_index].use_root_translation ? models[team_index].root_translation[0] : root_limb.translation[0],
                         models[team_index].use_root_translation ? models[team_index].root_translation[1] : root_limb.translation[1],
                         models[team_index].use_root_translation ? models[team_index].root_translation[2] : root_limb.translation[2]);
        matrix_rotate_z(parent_matrix,
                        builder.joint_angles[root_joint_index][2]);
        matrix_rotate_y(parent_matrix,
                        builder.joint_angles[root_joint_index][1]);
        matrix_rotate_x(parent_matrix,
                        builder.joint_angles[root_joint_index][0]);

        if (root_limb.display_list != 0U) {
            if (current_display_lists >= SF64_TITLE_TEAM_MATRIX_LIMIT) {
                return -1;
            }
            matrix_multiply(final_matrix, world_matrix, parent_matrix);
            matrix_write_n64(rdram + SF64_TITLE_TEAM_MATRIX_OFFSET +
                                 current_display_lists *
                                     SF64_ARWING_G_MTX_BYTES,
                             final_matrix);
            list_append(&builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                        SF64_TITLE_TEAM_MATRIX_ADDRESS +
                            current_display_lists * SF64_ARWING_G_MTX_BYTES);
            list_append(&builder, SF64_ARWING_G_DL, root_limb.display_list);
            current_display_lists++;
            builder.display_list_count = current_display_lists;
        }
        if (root_limb.child != 0U &&
            emit_limb(&builder, root_limb.child, &parent_matrix,
                      &world_matrix) != 0) {
            return -1;
        }
        command_count = builder.root_command_count;
        current_display_lists = builder.display_list_count;
        total_limbs += skeleton_limb_count;
    }

    list_append(&(Sf64ArwingBuilder){
                    .rdram = rdram,
                    .root_command_count = command_count,
                },
                SF64_ARWING_G_ENDDL, 0U);
    task->display_list_size_bytes =
        (command_count + 1U) * SF64_ARWING_GFX_COMMAND_BYTES;
    task->debug_name = "SF64 title starfield, team, Arwing, and logos";
    if (limb_count) {
        *limb_count = total_limbs;
    }
    if (display_list_count) {
        *display_list_count = current_display_lists;
    }
    return 0;
}

int sf64_arwing_append_title_teams(N64ps3GfxTask *task, const u8 *title_segment,
                                   u32 title_segment_size, const Sf64TitleTeamModel models[4],
                                   const Sf64OriginalTitleLighting *lighting,
                                   u32 *limb_count, u32 *display_list_count)
{
    return append_title_teams_view(task, title_segment, title_segment_size, models,
                                   lighting, limb_count, display_list_count, NULL, NULL, NULL);
}

static int team_running_append_card(N64ps3GfxTask *task, const u8 *title,
                                    u32 title_size, int team_index,
                                    u8 *rdram);
static int team_running_append_shadows(N64ps3GfxTask *task,
                                       const Sf64TitleTeamModel models[4],
                                       const float eye[3], const float at[3],
                                       const float up[3], float team_z_offset,
                                       u8 *rdram);

int sf64_team_running_build(N64ps3GfxTask *task, u8 *rdram, u32 rdram_size,
                            const u8 *title, u32 title_size,
                            const float eye[3], const float at[3], const float up[3],
                            float team_z_offset, float passage_offset, float fade,
                            const float light_dir[3], const float ambient[3],
                            const int frames[4], int team_name_visible,
                            int team_name_index)
{
    static const float positions[4][3] = {
        {-20.0f, 0.0f, -600.0f}, {-209.0f, 0.0f, -820.0f},
        {62.0f, 0.0f, -813.0f}, {183.0f, 0.0f, -990.0f}
    };
    Sf64TitleTeamModel models[4];
    Sf64OriginalTitleLighting lighting;
    Sf64ArwingMatrix projection, view, passage;
    Sf64ArwingBuilder builder;
    u32 index;
    float z;

    if (!task || !rdram || rdram_size < SF64_ARWING_RDRAM_REQUIRED_SIZE ||
        !title || !eye || !at || !up || !frames) return -1;
    if (fade < 0.0f) fade = 0.0f;
    if (fade > 1.0f) fade = 1.0f;
    memset(rdram, 0, rdram_size);
    memset(models, 0, sizeof(models));
    memset(&lighting, 0, sizeof(lighting));
    for (index = 0; index < 4U; index++) {
        if (sf64_title_running_animation(title, title_size, index, (u32)frames[index],
                                          &models[index], models[index].root_translation) != 0) return -1;
        models[index].use_root_translation = 1;
        models[index].position[0] = positions[index][0];
        models[index].position[1] = positions[index][1];
        models[index].position[2] = positions[index][2] + team_z_offset;
        models[index].scale = 0.5f;
    }
    lighting.directional_rgb[0] = (u8)(76.0f * fade);
    lighting.directional_rgb[1] = (u8)(45.0f * fade);
    lighting.directional_rgb[2] = (u8)(66.0f * fade);
    lighting.ambient_rgb[0] = (u8)(11.0f * fade);
    lighting.ambient_rgb[1] = (u8)(1.0f * fade);
    lighting.ambient_rgb[2] = (u8)(1.0f * fade);
    lighting.direction[0] = light_dir ? light_dir[0] : -100.0f;
    lighting.direction[1] = light_dir ? light_dir[1] : 0.0f;
    lighting.direction[2] = light_dir ? light_dir[2] : 0.0f;

    matrix_look_at(view, eye[0], eye[1], eye[2], at[0], at[1], at[2], up[0], up[1], up[2]);
    matrix_perspective(projection, 1.0f);
    matrix_write_n64(rdram + SF64_ARWING_PROJECTION_OFFSET, projection);
    matrix_write_n64(rdram + SF64_ARWING_VIEW_MATRIX_OFFSET, view);
    /* Title_Passage_Draw: unlit walls shaded only by the cycling ambient. */
    memset(rdram + 0x8200U, 0, 16U);
    rdram[0x8210U] = (u8)((ambient ? ambient[0] : 77.0f) * fade);
    rdram[0x8211U] = (u8)((ambient ? ambient[1] : 77.0f) * fade);
    rdram[0x8212U] = (u8)((ambient ? ambient[2] : 77.0f) * fade);
    memset(&builder, 0, sizeof(builder));
    builder.rdram = rdram;
    list_append(&builder, 0xbc001806U, 0x80030000U);
    list_append(&builder, SF64_ARWING_G_MTX_PROJECTION_LOAD, SF64_ARWING_PROJECTION_ADDRESS);
    list_append(&builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD, SF64_ARWING_VIEW_MATRIX_ADDRESS);
    list_append(&builder, SF64_ARWING_G_MOVEWORD_NUMLIGHT, 0x00000040U);
    list_append(&builder, SF64_ARWING_G_MOVEMEM_LIGHT, 0x80008200U);
    list_append(&builder, SF64_ARWING_G_MOVEMEM_AMBIENT, 0x80008210U);
    list_append(&builder, SF64_ARWING_G_TEXTURE, 0xffffffffU);
    list_append(&builder, SF64_ARWING_G_SETGEOMETRYMODE, SF64_ARWING_G_LIGHTING_ZBUFFER);
    /* Black fog for the passage and team; matches RCP_SetupDL_29(..., 995, 1000). */
    list_append(&builder, 0xb900031dU, 0xc0000000U);
    list_append(&builder, 0xf8000000U, 0x000000ffU);
    list_append(&builder, 0xbc000008U, 0x64009d00U);
    z = 3079.2002f * (at[2] < eye[2] ? 1.0f : 3.0f) - passage_offset;
    for (index = 0; index < 3U; index++) {
        memcpy(passage, view, sizeof(passage));
        matrix_translate(passage, 0.0f, 0.0f, z - (float)index * 4101.6f);
        matrix_scale(passage, 0.6f, 0.6f, 0.6f);
        matrix_write_n64(rdram + 0x8400U + index * 64U, passage);
        list_append(&builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD, 0x80008400U + index * 64U);
        list_append(&builder, SF64_ARWING_G_DL, 0x0602e380U);
    }
    list_append(&builder, SF64_ARWING_G_ENDDL, 0);
    memset(task, 0, sizeof(*task));
    task->kind = N64PS3_GFX_TASK_DISPLAY_LIST;
    task->dram_base = rdram;
    task->dram_size_bytes = rdram_size;
    task->display_list = rdram + SF64_ARWING_ROOT_LIST_OFFSET;
    task->display_list_size_bytes = builder.root_command_count * SF64_ARWING_GFX_COMMAND_BYTES;
    task->debug_name = "SF64 opening team running";
    if (append_title_teams_view(task, title, title_size, models, &lighting,
                                NULL, NULL, eye, at, up) != 0) {
        return -1;
    }
    if (team_running_append_shadows(task, models, eye, at, up, team_z_offset,
                                    rdram) != 0) {
        return -1;
    }
    if (team_name_visible && team_name_index >= 0 && team_name_index < 4) {
        if (team_running_append_card(task, title, title_size, team_name_index,
                                     rdram) != 0) {
            return -1;
        }
    }
    return 0;
}

#define SF64_TAKEOFF_ROOT_OFFSET 0x00000100U
#define SF64_TAKEOFF_MATRIX_OFFSET 0x00007000U
#define SF64_TAKEOFF_MATRIX_ADDRESS 0x80007000U
#define SF64_TAKEOFF_LIGHT_OFFSET 0x00008000U
#define SF64_TAKEOFF_LIGHT_ADDRESS 0x80008000U
#define SF64_TAKEOFF_PROJECTION_OFFSET 0x00008040U
#define SF64_TAKEOFF_PROJECTION_ADDRESS 0x80008040U
#define SF64_TAKEOFF_VIEW_OFFSET 0x00008080U
#define SF64_TAKEOFF_VIEW_ADDRESS 0x80008080U
#define SF64_TAKEOFF_ORTHO_OFFSET 0x00008200U
#define SF64_TAKEOFF_ORTHO_ADDRESS 0x80008200U
#define SF64_TAKEOFF_IDENTITY_OFFSET 0x00008240U
#define SF64_TAKEOFF_IDENTITY_ADDRESS 0x80008240U
#define SF64_TAKEOFF_OBJECT_MATRIX_OFFSET 0x00008800U
#define SF64_TAKEOFF_OBJECT_MATRIX_ADDRESS 0x80008800U
#define SF64_TAKEOFF_CARD_TEXTURE_OFFSET 0x00084000U
#define SF64_TAKEOFF_CARD_VERTEX_OFFSET 0x00087000U
#define SF64_TAKEOFF_FLASH_TEXTURE_OFFSET 0x00083f00U
#define SF64_TAKEOFF_FLASH_VERTEX_OFFSET 0x00087400U
#define SF64_TAKEOFF_GLOW_TEXTURE_OFFSET 0x00087800U
#define SF64_TAKEOFF_GLOW_VERTEX_OFFSET 0x00088840U
#define SF64_TAKEOFF_GLOW_SEGMENT_OFFSET 0x00032178U
#define SF64_TAKEOFF_SHADOW_TEXTURE_OFFSET 0x00088880U
#define SF64_TAKEOFF_SHADOW_VERTEX_OFFSET 0x00089880U
#define SF64_TAKEOFF_SHADOW_SEGMENT_OFFSET 0x00032608U

/* Cockpit face and canopy models live in the Arwing segment */
#define SF64_ARWING_COCKPIT_GLASS_DL 0x030194e0U
#define SF64_ARWING_COCKPIT_GLASS_VERTEX_OFFSET 0x00019548U
#define SF64_ARWING_COCKPIT_GLASS_VERTEX_COUNT 10U
#define SF64_ARWING_COCKPIT_GLASS_ALPHA 140U
static const u32 s_arwing_face_dls[4] = {
    0x03000090U, /* Fox */
    0x03016cc0U, /* Falco */
    0x03005ab0U, /* Slippy */
    0x03003ce0U  /* Peppy */
};
#define SF64_TAKEOFF_G_SETTIMG_RGBA16 0xfd100000U
#define SF64_TAKEOFF_G_SETTILE_RGBA16 0xf5100000U
#define SF64_TAKEOFF_DECK_DL 0x06018d40U
#define SF64_TAKEOFF_DECK_PLATFORM_DL 0x0602a720U
#define SF64_TAKEOFF_DECK_LAUNCHER_DL 0x0601c7c0U
#define SF64_TAKEOFF_CARD_TEXTURE_SEGMENT_OFFSET 0x00011890U
#define SF64_TAKEOFF_CARD_WIDTH 112U
#define SF64_TAKEOFF_CARD_HEIGHT 26U
#define SF64_TAKEOFF_CARD_X 101
#define SF64_TAKEOFF_CARD_Y 197
#define SF64_TAKEOFF_SCREEN_WIDTH 320.0f
#define SF64_TAKEOFF_SCREEN_HEIGHT 240.0f
#define SF64_TAKEOFF_G_SETTIMG_RGBA32 0xfd180000U
#define SF64_TAKEOFF_G_SETTILE_RGBA32 0xf5180000U
#define SF64_TAKEOFF_G_LOADBLOCK 0xf3000000U
#define SF64_TAKEOFF_G_LOADSYNC 0xe6000000U
#define SF64_TAKEOFF_G_PIPESYNC 0xe7000000U
#define SF64_TAKEOFF_G_SETTILESIZE 0xf2000000U
#define SF64_TAKEOFF_G_TRI1 0xbf000000U
#define SF64_TAKEOFF_G_VTX 0x04000000U
#define SF64_TAKEOFF_G_CLEARGEOMETRYMODE 0xb6000000U
#define SF64_TAKEOFF_G_SETOTHERMODE_H 0xe3000c02U
#define SF64_TAKEOFF_G_SETOTHERMODE_L 0xb900031dU
#define SF64_TAKEOFF_G_MOVEWORD_SEGMENT7 0xbc001c06U
#define SF64_TAKEOFF_TEXTURE_CLAMP_ST ((2U << 8) | (2U << 18))
#define SF64_TAKEOFF_TEXTURE_TILE_LOAD 7U
#define SF64_TAKEOFF_CLEAR_GEOMETRY_BITS 0x00023001U
#define SF64_TAKEOFF_GLOW_CLEAR_BITS 0x00023000U
#define SF64_TAKEOFF_TEXTURE_FILTER_BILERP 0x00002000U
#define SF64_TAKEOFF_RENDER_MODE_XLU 0x00504240U

static void takeoff_write_light(uint8_t *rdram, uint32_t offset,
                                const Sf64TakeoffState *state, int directional)
{
    uint8_t *light = rdram + offset;
    uint8_t *ambient = light + 16;

    memset(light, 0, 16);
    memset(ambient, 0, 16);
    if (directional) {
        light[0] = (uint8_t)state->light1_r;
        light[1] = (uint8_t)state->light1_g;
        light[2] = (uint8_t)state->light1_b;
    }
    light[8] = (uint8_t)((int32_t)state->light_dir[0] & 0xff);
    light[9] = (uint8_t)((int32_t)state->light_dir[1] & 0xff);
    light[10] = (uint8_t)((int32_t)state->light_dir[2] & 0xff);
    ambient[0] = (uint8_t)state->ambient_r;
    ambient[1] = (uint8_t)state->ambient_g;
    ambient[2] = (uint8_t)state->ambient_b;
}

static void takeoff_ortho_matrix(Sf64ArwingMatrix matrix)
{
    float scale_x;
    float scale_y;

    memset(matrix, 0, sizeof(Sf64ArwingMatrix));
    sf64_display_fit_4_3(sf64_display_aspect(), &scale_x, &scale_y);
    matrix[0][0] = (2.0f / SF64_TAKEOFF_SCREEN_WIDTH) * scale_x;
    matrix[1][1] = (-2.0f / SF64_TAKEOFF_SCREEN_HEIGHT) * scale_y;
    matrix[2][2] = 1.0f;
    matrix[3][0] = -scale_x;
    matrix[3][1] = scale_y;
    matrix[3][3] = 1.0f;
}

static void takeoff_write_vertex(uint8_t *destination, int16_t x, int16_t y,
                                 int16_t s, int16_t t)
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

static void takeoff_append_card(Sf64ArwingBuilder *builder,
                                const uint8_t *title_segment,
                                uint8_t *rdram)
{
    Sf64ArwingMatrix ortho;
    Sf64ArwingMatrix identity;
    uint8_t *texture = rdram + SF64_TAKEOFF_CARD_TEXTURE_OFFSET;
    uint8_t *vertices = rdram + SF64_TAKEOFF_CARD_VERTEX_OFFSET;
    uint32_t pixel;

    for (pixel = 0; pixel < SF64_TAKEOFF_CARD_WIDTH * SF64_TAKEOFF_CARD_HEIGHT;
         pixel++) {
        uint8_t ia8 = title_segment[SF64_TAKEOFF_CARD_TEXTURE_SEGMENT_OFFSET +
                                    pixel];
        uint8_t intensity = (uint8_t)((ia8 >> 4) * 17U);
        uint8_t alpha = (uint8_t)((ia8 & 0x0fU) * 17U);

        if (alpha <= 32U) {
            alpha = 0U;
        } else {
            alpha = (uint8_t)((((uint32_t)(alpha - 32U) * 255U) + 111U) / 223U);
        }
        texture[pixel * 4U] = intensity;
        texture[pixel * 4U + 1U] = intensity;
        texture[pixel * 4U + 2U] = intensity;
        texture[pixel * 4U + 3U] = alpha;
    }

    takeoff_ortho_matrix(ortho);
    matrix_identity(identity);
    matrix_write_n64(rdram + SF64_TAKEOFF_ORTHO_OFFSET, ortho);
    matrix_write_n64(rdram + SF64_TAKEOFF_IDENTITY_OFFSET, identity);

    list_append(builder, SF64_ARWING_G_MTX_PROJECTION_LOAD,
                SF64_TAKEOFF_ORTHO_ADDRESS);
    list_append(builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                SF64_TAKEOFF_IDENTITY_ADDRESS);
    list_append(builder, SF64_TAKEOFF_G_CLEARGEOMETRYMODE,
                SF64_TAKEOFF_CLEAR_GEOMETRY_BITS);
    list_append(builder, SF64_ARWING_G_TEXTURE, 0xffffffffU);
    list_append(builder, SF64_TAKEOFF_G_SETOTHERMODE_H,
                0x00000000U);
    list_append(builder, SF64_TAKEOFF_G_SETOTHERMODE_L,
                SF64_TAKEOFF_RENDER_MODE_XLU);
    list_append(builder, SF64_TAKEOFF_G_MOVEWORD_SEGMENT7,
                0x80000000U | SF64_TAKEOFF_CARD_TEXTURE_OFFSET);
    list_append(builder, SF64_TAKEOFF_G_PIPESYNC, 0U);
    list_append(builder, SF64_TAKEOFF_G_SETTIMG_RGBA32 |
                             (SF64_TAKEOFF_CARD_WIDTH - 1U),
                0x07000000U);
    list_append(builder, SF64_TAKEOFF_G_SETTILE_RGBA32,
                (SF64_TAKEOFF_TEXTURE_TILE_LOAD << 24) |
                    SF64_TAKEOFF_TEXTURE_CLAMP_ST);
    list_append(builder, SF64_TAKEOFF_G_LOADSYNC, 0U);
    list_append(builder, SF64_TAKEOFF_G_LOADBLOCK,
                (SF64_TAKEOFF_TEXTURE_TILE_LOAD << 24) |
                    ((SF64_TAKEOFF_CARD_WIDTH * SF64_TAKEOFF_CARD_HEIGHT - 1U)
                     << 12));
    list_append(builder, SF64_TAKEOFF_G_PIPESYNC, 0U);
    list_append(builder, SF64_TAKEOFF_G_SETTILE_RGBA32 |
                             (((SF64_TAKEOFF_CARD_WIDTH * 4U + 7U) / 8U) << 9),
                SF64_TAKEOFF_TEXTURE_CLAMP_ST);
    list_append(builder, SF64_TAKEOFF_G_SETTILESIZE,
                (((SF64_TAKEOFF_CARD_WIDTH - 1U) * 4U) << 12) |
                    ((SF64_TAKEOFF_CARD_HEIGHT - 1U) * 4U));

    takeoff_write_vertex(vertices, SF64_TAKEOFF_CARD_X, SF64_TAKEOFF_CARD_Y, 0, 0);
    takeoff_write_vertex(vertices + 16U, SF64_TAKEOFF_CARD_X + (int)SF64_TAKEOFF_CARD_WIDTH,
                         SF64_TAKEOFF_CARD_Y, SF64_TAKEOFF_CARD_WIDTH * 32, 0);
    takeoff_write_vertex(vertices + 32U, SF64_TAKEOFF_CARD_X + (int)SF64_TAKEOFF_CARD_WIDTH,
                         SF64_TAKEOFF_CARD_Y + (int)SF64_TAKEOFF_CARD_HEIGHT,
                         SF64_TAKEOFF_CARD_WIDTH * 32, SF64_TAKEOFF_CARD_HEIGHT * 32);
    takeoff_write_vertex(vertices + 48U, SF64_TAKEOFF_CARD_X,
                         SF64_TAKEOFF_CARD_Y + (int)SF64_TAKEOFF_CARD_HEIGHT,
                         0, SF64_TAKEOFF_CARD_HEIGHT * 32);

    list_append(builder, SF64_TAKEOFF_G_VTX | (4U << 10) | (4U * 16U - 1U),
                0x80000000U | SF64_TAKEOFF_CARD_VERTEX_OFFSET);
    list_append(builder, SF64_TAKEOFF_G_TRI1, 0x00000204U);
    list_append(builder, SF64_TAKEOFF_G_TRI1, 0x00000406U);
}

static void takeoff_append_flash(Sf64ArwingBuilder *builder,
                                 const Sf64TakeoffState *state,
                                 uint8_t *rdram)
{
    Sf64ArwingMatrix ortho;
    Sf64ArwingMatrix identity;
    uint8_t *texture = rdram + SF64_TAKEOFF_FLASH_TEXTURE_OFFSET;
    uint8_t *vertices = rdram + SF64_TAKEOFF_FLASH_VERTEX_OFFSET;
    int alpha = state->fill_alpha;
    int i;

    if (alpha <= 0) {
        return;
    }
    if (alpha > 255) {
        alpha = 255;
    }

    texture[0] = 0xffU;
    texture[1] = 0xffU;
    takeoff_ortho_matrix(ortho);
    matrix_identity(identity);
    matrix_write_n64(rdram + SF64_TAKEOFF_ORTHO_OFFSET, ortho);
    matrix_write_n64(rdram + SF64_TAKEOFF_IDENTITY_OFFSET, identity);

    list_append(builder, SF64_ARWING_G_MTX_PROJECTION_LOAD,
                SF64_TAKEOFF_ORTHO_ADDRESS);
    list_append(builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                SF64_TAKEOFF_IDENTITY_ADDRESS);
    list_append(builder, SF64_TAKEOFF_G_CLEARGEOMETRYMODE,
                SF64_TAKEOFF_CLEAR_GEOMETRY_BITS);
    list_append(builder, SF64_ARWING_G_TEXTURE, 0xffffffffU);
    list_append(builder, SF64_TAKEOFF_G_SETOTHERMODE_H,
                SF64_TAKEOFF_TEXTURE_FILTER_BILERP);
    list_append(builder, SF64_TAKEOFF_G_SETOTHERMODE_L,
                SF64_TAKEOFF_RENDER_MODE_XLU);
    list_append(builder, SF64_TAKEOFF_G_MOVEWORD_SEGMENT7,
                0x80000000U | SF64_TAKEOFF_FLASH_TEXTURE_OFFSET);
    list_append(builder, SF64_TAKEOFF_G_PIPESYNC, 0U);
    list_append(builder, SF64_TAKEOFF_G_SETTIMG_RGBA16, 0x07000000U);
    list_append(builder, SF64_TAKEOFF_G_SETTILE_RGBA16,
                (SF64_TAKEOFF_TEXTURE_TILE_LOAD << 24) |
                    SF64_TAKEOFF_TEXTURE_CLAMP_ST);
    list_append(builder, SF64_TAKEOFF_G_LOADSYNC, 0U);
    list_append(builder, SF64_TAKEOFF_G_LOADBLOCK,
                SF64_TAKEOFF_TEXTURE_TILE_LOAD << 24);
    list_append(builder, SF64_TAKEOFF_G_PIPESYNC, 0U);
    list_append(builder, SF64_TAKEOFF_G_SETTILE_RGBA16 |
                             (((1U * 2U + 7U) / 8U) << 9),
                SF64_TAKEOFF_TEXTURE_CLAMP_ST);
    list_append(builder, SF64_TAKEOFF_G_SETTILESIZE, 0U);

    takeoff_write_vertex(vertices, 0, 0, 0, 0);
    takeoff_write_vertex(vertices + 16U, 320, 0, 0, 0);
    takeoff_write_vertex(vertices + 32U, 320, 240, 0, 0);
    takeoff_write_vertex(vertices + 48U, 0, 240, 0, 0);
    for (i = 0; i < 4; i++) {
        uint8_t *vertex = vertices + (uint32_t)i * 16U;
        vertex[12] = (uint8_t)state->fill_red;
        vertex[13] = (uint8_t)state->fill_green;
        vertex[14] = (uint8_t)state->fill_blue;
        vertex[15] = (uint8_t)alpha;
    }

    list_append(builder, SF64_TAKEOFF_G_VTX | (4U << 10) | (4U * 16U - 1U),
                0x80000000U | SF64_TAKEOFF_FLASH_VERTEX_OFFSET);
    list_append(builder, SF64_TAKEOFF_G_TRI1, 0x00000204U);
    list_append(builder, SF64_TAKEOFF_G_TRI1, 0x00000406U);
}

static void takeoff_setup_engine_glow(Sf64ArwingBuilder *builder,
                                      const uint8_t *title_segment,
                                      uint8_t *rdram,
                                      uint32_t texture_offset,
                                      uint32_t vertex_offset)
{
    uint8_t *texture = rdram + texture_offset;
    uint8_t *vertices = rdram + vertex_offset;
    uint32_t pixel;
    uint32_t i;

    for (pixel = 0; pixel < 32U * 32U; pixel++) {
        uint8_t ia8 = title_segment[SF64_TAKEOFF_GLOW_SEGMENT_OFFSET + pixel];
        /* Flat white RGB: the source intensity is the alpha shape. Feeding
         * intensity into RGB made zero-intensity texels render opaque black. */
        texture[pixel * 4U] = 255U;
        texture[pixel * 4U + 1U] = 255U;
        texture[pixel * 4U + 2U] = 255U;
        texture[pixel * 4U + 3U] = (uint8_t)((ia8 & 0x0fU) * 17U);
    }

    list_append(builder, SF64_TAKEOFF_G_CLEARGEOMETRYMODE,
                SF64_TAKEOFF_GLOW_CLEAR_BITS);
    list_append(builder, SF64_ARWING_G_TEXTURE, 0xffffffffU);
    list_append(builder, SF64_TAKEOFF_G_SETOTHERMODE_H,
                SF64_TAKEOFF_TEXTURE_FILTER_BILERP);
    list_append(builder, SF64_TAKEOFF_G_SETOTHERMODE_L,
                SF64_TAKEOFF_RENDER_MODE_XLU);
    list_append(builder, SF64_TAKEOFF_G_MOVEWORD_SEGMENT7,
                0x80000000U | texture_offset);
    list_append(builder, SF64_TAKEOFF_G_PIPESYNC, 0U);
    list_append(builder, SF64_TAKEOFF_G_SETTIMG_RGBA32 | 31U, 0x07000000U);
    list_append(builder, SF64_TAKEOFF_G_SETTILE_RGBA32,
                (SF64_TAKEOFF_TEXTURE_TILE_LOAD << 24) |
                    SF64_TAKEOFF_TEXTURE_CLAMP_ST);
    list_append(builder, SF64_TAKEOFF_G_LOADSYNC, 0U);
    list_append(builder, SF64_TAKEOFF_G_LOADBLOCK,
                (SF64_TAKEOFF_TEXTURE_TILE_LOAD << 24) |
                    ((32U * 32U - 1U) << 12));
    list_append(builder, SF64_TAKEOFF_G_PIPESYNC, 0U);
    list_append(builder, SF64_TAKEOFF_G_SETTILE_RGBA32 |
                             (((32U * 4U + 7U) / 8U) << 9),
                SF64_TAKEOFF_TEXTURE_CLAMP_ST);
    list_append(builder, SF64_TAKEOFF_G_SETTILESIZE,
                (124U << 12) | 124U);

    takeoff_write_vertex(vertices, -40, 40, 0, 0);
    takeoff_write_vertex(vertices + 16U, -40, -40, 0, 1024);
    takeoff_write_vertex(vertices + 32U, 40, -40, 1024, 1024);
    takeoff_write_vertex(vertices + 48U, 40, 40, 1024, 0);
    for (i = 0; i < 4U; i++) {
        uint8_t *vertex = vertices + i * 16U;
        vertex[12] = 128U;
        vertex[13] = 128U;
        vertex[14] = 255U;
        vertex[15] = 255U;
    }
}

static void takeoff_append_engine_glow(Sf64ArwingBuilder *builder,
                                       const Sf64TakeoffArwing *arwing,
                                       const float eye[3],
                                       Sf64ArwingMatrix view,
                                       uint32_t slot,
                                       uint32_t vertex_offset)
{
    const float degrees_to_radians = SF64_ARWING_TWO_PI / 360.0f;
    float source_offset;
    float camera_y;
    float distance;
    float camera_x;
    Sf64ArwingMatrix matrix;

    source_offset = arwing->engine_particle == 1 ? -30.0f : -65.0f;
    camera_y = -atan2f(eye[0] - arwing->position[0],
                       eye[2] - arwing->position[2]);
    distance = sqrtf((eye[2] - arwing->position[2]) *
                         (eye[2] - arwing->position[2]) +
                     (eye[0] - arwing->position[0]) *
                         (eye[0] - arwing->position[0]));
    camera_x = atan2f(eye[1] - arwing->position[1], distance);

    memcpy(matrix, view, sizeof(matrix));
    matrix_translate(matrix, arwing->position[0], arwing->position[1],
                     arwing->position[2]);
    matrix_scale(matrix, arwing->scale, arwing->scale, arwing->scale);
    matrix_rotate_z(matrix, arwing->rotation_degrees[2] * degrees_to_radians);
    matrix_rotate_x(matrix, arwing->rotation_degrees[0] * degrees_to_radians);
    matrix_rotate_y(matrix, arwing->rotation_degrees[1] * degrees_to_radians);
    matrix_translate(matrix, 0.0f, 0.0f, source_offset);
    matrix_scale(matrix, arwing->engine_glow_scale,
                 arwing->engine_glow_scale * 0.7f,
                 arwing->engine_glow_scale);
    matrix_rotate_z(matrix, -arwing->rotation_degrees[2] * degrees_to_radians);
    matrix_rotate_x(matrix, -arwing->rotation_degrees[0] * degrees_to_radians);
    matrix_rotate_y(matrix, -arwing->rotation_degrees[1] * degrees_to_radians);
    matrix_rotate_y(matrix, -camera_y);
    matrix_rotate_x(matrix, -camera_x);

    matrix_write_n64(builder->rdram + SF64_TAKEOFF_OBJECT_MATRIX_OFFSET +
                         slot * 64U,
                     matrix);
    list_append(builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                SF64_TAKEOFF_OBJECT_MATRIX_ADDRESS + slot * 64U);
    list_append(builder, SF64_TAKEOFF_G_VTX | (4U << 10) | (4U * 16U - 1U),
                0x80000000U | vertex_offset);
    list_append(builder, SF64_TAKEOFF_G_TRI1, 0x00000204U);
    list_append(builder, SF64_TAKEOFF_G_TRI1, 0x00000406U);
}

static void takeoff_write_vertex3(uint8_t *destination, int16_t x, int16_t y,
                                  int16_t z, int16_t s, int16_t t)
{
    write_be16(destination, (uint16_t)x);
    write_be16(destination + 2U, (uint16_t)y);
    write_be16(destination + 4U, (uint16_t)z);
    write_be16(destination + 6U, 0U);
    write_be16(destination + 8U, (uint16_t)s);
    write_be16(destination + 10U, (uint16_t)t);
    destination[12] = 255U;
    destination[13] = 255U;
    destination[14] = 255U;
    destination[15] = 240U;
}

static void takeoff_setup_shadow(Sf64ArwingBuilder *builder,
                                 const uint8_t *title_segment,
                                 uint8_t *rdram)
{
    uint8_t *texture = rdram + SF64_TAKEOFF_SHADOW_TEXTURE_OFFSET;
    uint8_t *vertices = rdram + SF64_TAKEOFF_SHADOW_VERTEX_OFFSET;
    uint32_t pixel;

    for (pixel = 0; pixel < 32U * 32U; pixel++) {
        uint16_t packed =
            (uint16_t)(((uint16_t)title_segment[SF64_TAKEOFF_SHADOW_SEGMENT_OFFSET +
                                               pixel * 2U]
                        << 8) |
                       title_segment[SF64_TAKEOFF_SHADOW_SEGMENT_OFFSET +
                                     pixel * 2U + 1U]);
        uint8_t intensity = (uint8_t)(packed >> 8);
        uint8_t alpha = (uint8_t)(packed & 0xffU);

        texture[pixel * 4U] = intensity;
        texture[pixel * 4U + 1U] = intensity;
        texture[pixel * 4U + 2U] = intensity;
        texture[pixel * 4U + 3U] = alpha;
    }

    list_append(builder, SF64_TAKEOFF_G_CLEARGEOMETRYMODE, 0x00020000U);
    list_append(builder, SF64_ARWING_G_TEXTURE, 0xffffffffU);
    list_append(builder, SF64_TAKEOFF_G_SETOTHERMODE_H,
                SF64_TAKEOFF_TEXTURE_FILTER_BILERP);
    list_append(builder, SF64_TAKEOFF_G_SETOTHERMODE_L,
                SF64_TAKEOFF_RENDER_MODE_XLU);
    list_append(builder, SF64_TAKEOFF_G_MOVEWORD_SEGMENT7,
                0x80000000U | SF64_TAKEOFF_SHADOW_TEXTURE_OFFSET);
    list_append(builder, SF64_TAKEOFF_G_PIPESYNC, 0U);
    list_append(builder, SF64_TAKEOFF_G_SETTIMG_RGBA32 | 31U, 0x07000000U);
    list_append(builder, SF64_TAKEOFF_G_SETTILE_RGBA32,
                SF64_TAKEOFF_TEXTURE_TILE_LOAD << 24);
    list_append(builder, SF64_TAKEOFF_G_LOADSYNC, 0U);
    list_append(builder, SF64_TAKEOFF_G_LOADBLOCK,
                (SF64_TAKEOFF_TEXTURE_TILE_LOAD << 24) |
                    ((32U * 32U - 1U) << 12));
    list_append(builder, SF64_TAKEOFF_G_PIPESYNC, 0U);
    list_append(builder,
                SF64_TAKEOFF_G_SETTILE_RGBA32 |
                    (((32U * 4U + 7U) / 8U) << 9),
                0U);
    list_append(builder, SF64_TAKEOFF_G_SETTILESIZE, (124U << 12) | 124U);

    takeoff_write_vertex3(vertices, 0, 0, -80, 512, -256);
    takeoff_write_vertex3(vertices + 16U, -100, 0, 20, -768, 1024);
    takeoff_write_vertex3(vertices + 32U, 100, 0, 20, 1792, 1024);
}

static void takeoff_append_shadow(Sf64ArwingBuilder *builder,
                                  const Sf64TakeoffArwing *arwing,
                                  const Sf64TakeoffState *state,
                                  Sf64ArwingMatrix view,
                                  uint32_t slot)
{
    const float degrees_to_radians = SF64_ARWING_TWO_PI / 360.0f;
    float shadow_y = (state->deck_shadow - arwing->position[1] * 2.05f) +
                     (state->deck_offset_y - 84.0f) * 1.99f;
    Sf64ArwingMatrix matrix;

    memcpy(matrix, view, sizeof(matrix));
    matrix_translate(matrix, arwing->position[0], arwing->position[1],
                     arwing->position[2]);
    matrix_scale(matrix, arwing->scale, arwing->scale, arwing->scale);
    matrix_rotate_z(matrix, arwing->rotation_degrees[2] * degrees_to_radians);
    matrix_rotate_x(matrix, arwing->rotation_degrees[0] * degrees_to_radians);
    matrix_rotate_y(matrix, arwing->rotation_degrees[1] * degrees_to_radians);
    matrix_translate(matrix, 0.0f, shadow_y, 0.0f);
    matrix_rotate_y(matrix, 3.14159265358979323846f);
    matrix_write_n64(builder->rdram + SF64_TAKEOFF_OBJECT_MATRIX_OFFSET +
                         slot * 64U,
                     matrix);
    list_append(builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                SF64_TAKEOFF_OBJECT_MATRIX_ADDRESS + slot * 64U);
    list_append(builder, SF64_TAKEOFF_G_VTX | (3U << 10) | (3U * 16U - 1U),
                0x80000000U | SF64_TAKEOFF_SHADOW_VERTEX_OFFSET);
    list_append(builder, SF64_TAKEOFF_G_TRI1, 0x00000204U);
}

/* The canopy uses the prim-colour alpha (140) for transparency; the DL carries
 * alpha 0, so give its vertices the prim alpha before the list is replayed. */
static void takeoff_patch_cockpit_glass(uint8_t *rdram)
{
    uint8_t *vertices = rdram + SF64_ARWING_SEGMENT_RDRAM_OFFSET +
                        SF64_ARWING_COCKPIT_GLASS_VERTEX_OFFSET;
    uint32_t i;

    for (i = 0; i < SF64_ARWING_COCKPIT_GLASS_VERTEX_COUNT; i++) {
        vertices[i * 16U + 15U] = SF64_ARWING_COCKPIT_GLASS_ALPHA;
    }
}

static void takeoff_emit_cockpit(Sf64ArwingBuilder *builder,
                                 const Sf64TakeoffArwing *pose,
                                 Sf64ArwingMatrix world,
                                 float degrees_to_radians)
{
    Sf64ArwingMatrix matrix;
    uint32_t slot;

    if (pose->draw_face >= 1 && pose->draw_face <= 4) {
        slot = builder->display_list_count;
        memcpy(matrix, world, sizeof(matrix));
        matrix_translate(matrix, 0.0f, 6.4f, -16.5f);
        matrix_rotate_y(matrix, pose->team_face_yrot * degrees_to_radians);
        matrix_rotate_x(matrix, pose->team_face_xrot * degrees_to_radians);
        matrix_scale(matrix, 1.0f / 70.925f, 1.0f / 70.925f, 1.0f / 70.925f);
        matrix_write_n64(builder->rdram + builder->matrix_offset + slot * 64U,
                         matrix);
        list_append(builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                    builder->matrix_address + slot * 64U);
        list_append(builder, SF64_ARWING_G_DL,
                    s_arwing_face_dls[pose->draw_face - 1]);
        builder->display_list_count++;
    }

    slot = builder->display_list_count;
    memcpy(matrix, world, sizeof(matrix));
    matrix_translate(matrix, 0.0f, 17.2f, -25.8f);
    matrix_rotate_x(matrix, pose->cockpit_glass_xrot * degrees_to_radians);
    matrix_write_n64(builder->rdram + builder->matrix_offset + slot * 64U,
                     matrix);
    list_append(builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                builder->matrix_address + slot * 64U);
    list_append(builder, SF64_TAKEOFF_G_SETOTHERMODE_L,
                SF64_TAKEOFF_RENDER_MODE_XLU);
    list_append(builder, SF64_ARWING_G_DL, SF64_ARWING_COCKPIT_GLASS_DL);
    list_append(builder, SF64_TAKEOFF_G_SETOTHERMODE_L, 0x00000000U);
    builder->display_list_count++;
}

static int takeoff_emit_arwing(Sf64ArwingBuilder *builder,
                               const Sf64ArwingLimb *root_limb,
                               u32 root_joint_index,
                               const Sf64TakeoffArwing *pose,
                               Sf64ArwingMatrix view)
{
    const float degrees_to_radians = SF64_ARWING_TWO_PI / 360.0f;
    Sf64ArwingMatrix world;
    Sf64ArwingMatrix root;
    Sf64ArwingMatrix final_matrix;

    matrix_identity(root);
    matrix_translate(root, root_limb->translation[0], root_limb->translation[1],
                     root_limb->translation[2]);
    matrix_rotate_z(root, builder->joint_angles[root_joint_index][2]);
    matrix_rotate_y(root, builder->joint_angles[root_joint_index][1]);
    matrix_rotate_x(root, builder->joint_angles[root_joint_index][0]);

    memcpy(world, view, sizeof(world));
    matrix_translate(world, pose->position[0], pose->position[1],
                     pose->position[2]);
    matrix_scale(world, pose->scale, pose->scale, pose->scale);
    matrix_rotate_z(world, pose->rotation_degrees[2] * degrees_to_radians);
    matrix_rotate_x(world, pose->rotation_degrees[0] * degrees_to_radians);
    matrix_rotate_y(world, pose->rotation_degrees[1] * degrees_to_radians);

    if (root_limb->display_list != 0U) {
        matrix_multiply(final_matrix, world, root);
        matrix_write_n64(builder->rdram + builder->matrix_offset +
                             builder->display_list_count * 64U,
                         final_matrix);
        list_append(builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                    builder->matrix_address + builder->display_list_count * 64U);
        list_append(builder, SF64_ARWING_G_DL, root_limb->display_list);
        builder->display_list_count++;
    }
    builder->visited_count = 0U;
    if (root_limb->child != 0U &&
        emit_limb(builder, root_limb->child, &root, &world) != 0) {
        return -1;
    }
    if (builder->display_list_count + 2U >= SF64_TITLE_TEAM_MATRIX_LIMIT) {
        return -1;
    }
    takeoff_emit_cockpit(builder, pose, world, degrees_to_radians);
    return 0;
}

int sf64_takeoff_build_task(N64ps3GfxTask *task,
                            uint8_t *rdram,
                            uint32_t rdram_size,
                            const uint8_t *arwing_segment,
                            uint32_t arwing_segment_size,
                            const uint8_t *title_segment,
                            uint32_t title_segment_size,
                            const Sf64TakeoffState *state)
{
    u32 skeleton[SF64_ARWING_MAX_LIMBS];
    float joint_angles[SF64_ARWING_MAX_LIMBS + 1U][3];
    u32 skeleton_limb_count;
    u32 animation_limb_count;
    u32 root_joint_index;
    Sf64ArwingLimb root_limb;
    Sf64ArwingMatrix projection;
    Sf64ArwingMatrix view;
    Sf64ArwingBuilder builder;
    u32 object_index = 0U;
    u32 arwing_index;

    if (!task || !rdram || !state || !arwing_segment || !title_segment ||
        arwing_segment_size < SF64_ARWING_SEGMENT_SIZE ||
        rdram_size < SF64_ARWING_RDRAM_REQUIRED_SIZE ||
        SF64_TITLE_SEGMENT_RDRAM_OFFSET + title_segment_size > rdram_size) {
        return -1;
    }

    memset(rdram, 0, rdram_size);
    memcpy(rdram + SF64_ARWING_SEGMENT_RDRAM_OFFSET, arwing_segment,
           SF64_ARWING_SEGMENT_SIZE);
    takeoff_patch_cockpit_glass(rdram);
    memcpy(rdram + SF64_TITLE_SEGMENT_RDRAM_OFFSET, title_segment,
           title_segment_size);

    if (skeleton_read(arwing_segment, arwing_segment_size, SF64_ARWING_SEGMENT_ID,
                      skeleton, &skeleton_limb_count) != 0 ||
        animation_read(arwing_segment, arwing_segment_size,
                       &animation_limb_count, joint_angles) != 0 ||
        skeleton_limb_count != animation_limb_count ||
        limb_read(&(Sf64ArwingBuilder){ .segment = arwing_segment,
                                        .segment_size = arwing_segment_size,
                                        .segment_id = SF64_ARWING_SEGMENT_ID },
                  skeleton[0], &root_limb) != 0 ||
        find_joint_index(&(Sf64ArwingBuilder){ .skeleton = skeleton },
                         skeleton[0], &root_joint_index) != 0) {
        return -1;
    }

    matrix_perspective(projection, 1.0f);
    matrix_look_at(view, state->eye[0], state->eye[1], state->eye[2],
                   state->at[0], state->at[1], state->at[2],
                   state->up[0], state->up[1], state->up[2]);
    matrix_write_n64(rdram + SF64_TAKEOFF_PROJECTION_OFFSET, projection);
    matrix_write_n64(rdram + SF64_TAKEOFF_VIEW_OFFSET, view);
    takeoff_write_light(rdram, SF64_TAKEOFF_LIGHT_OFFSET, state, 0);
    takeoff_write_light(rdram, SF64_TAKEOFF_LIGHT_OFFSET + 0x20U, state, 1);

    memset(&builder, 0, sizeof(builder));
    builder.segment = arwing_segment;
    builder.segment_size = arwing_segment_size;
    builder.segment_id = SF64_ARWING_SEGMENT_ID;
    builder.matrix_offset = SF64_TAKEOFF_MATRIX_OFFSET;
    builder.matrix_address = SF64_TAKEOFF_MATRIX_ADDRESS;
    builder.skeleton = skeleton;
    builder.joint_angles = joint_angles;
    builder.rdram = rdram;

    list_append(&builder, SF64_ARWING_G_MOVEWORD_SEGMENT,
                SF64_ARWING_SEGMENT_ADDRESS);
    list_append(&builder, 0xbc001806U, SF64_TITLE_SEGMENT_RDRAM_OFFSET |
                                           0x80000000U);
    list_append(&builder, SF64_ARWING_G_MTX_PROJECTION_LOAD,
                SF64_TAKEOFF_PROJECTION_ADDRESS);
    list_append(&builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                SF64_TAKEOFF_VIEW_ADDRESS);
    list_append(&builder, SF64_ARWING_G_MOVEWORD_NUMLIGHT, 0x00000040U);
    list_append(&builder, SF64_ARWING_G_TEXTURE, 0xffffffffU);
    list_append(&builder, SF64_ARWING_G_SETGEOMETRYMODE,
                SF64_ARWING_G_LIGHTING_ZBUFFER);
    list_append(&builder, SF64_ARWING_G_MOVEMEM_LIGHT,
                SF64_TAKEOFF_LIGHT_ADDRESS);
    list_append(&builder, SF64_ARWING_G_MOVEMEM_AMBIENT,
                SF64_TAKEOFF_LIGHT_ADDRESS + 16U);

    {
        Sf64ArwingMatrix deck;
        memcpy(deck, view, sizeof(deck));
        matrix_translate(deck, 0.0f, state->deck_offset_y,
                         state->deck_offset_z);
        matrix_scale(deck, 0.4f, 0.4f, 0.4f);
        matrix_write_n64(rdram + SF64_TAKEOFF_OBJECT_MATRIX_OFFSET, deck);
        list_append(&builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                    SF64_TAKEOFF_OBJECT_MATRIX_ADDRESS);
        list_append(&builder, SF64_ARWING_G_DL, SF64_TAKEOFF_DECK_DL);

        if (state->platform_visible) {
            Sf64ArwingMatrix platform;
            memcpy(platform, deck, sizeof(platform));
            matrix_translate(platform, 401.0f, -249.0f, -22.0f);
            matrix_write_n64(rdram + SF64_TAKEOFF_OBJECT_MATRIX_OFFSET + 64U,
                             platform);
            list_append(&builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                        SF64_TAKEOFF_OBJECT_MATRIX_ADDRESS + 64U);
            list_append(&builder, SF64_ARWING_G_DL,
                        SF64_TAKEOFF_DECK_PLATFORM_DL);
        }
    }
    object_index = 2U;

    for (arwing_index = 4U; arwing_index > 0U; arwing_index--) {
        u32 index = arwing_index - 1U;
        Sf64ArwingMatrix launcher;
        if (!state->arwing[index].draw) {
            continue;
        }
        memcpy(launcher, view, sizeof(launcher));
        matrix_translate(launcher, state->arwing[index].position[0], -12.8f,
                         state->deck_launcher_zpos);
        matrix_scale(launcher, 0.8f, 0.8f, 0.8f);
        matrix_write_n64(rdram + SF64_TAKEOFF_OBJECT_MATRIX_OFFSET +
                             object_index * 64U,
                         launcher);
        list_append(&builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                    SF64_TAKEOFF_OBJECT_MATRIX_ADDRESS + object_index * 64U);
        list_append(&builder, SF64_ARWING_G_DL, SF64_TAKEOFF_DECK_LAUNCHER_DL);
        object_index++;
    }

    list_append(&builder, SF64_ARWING_G_MOVEMEM_LIGHT,
                SF64_TAKEOFF_LIGHT_ADDRESS + 0x20U);
    list_append(&builder, SF64_ARWING_G_MOVEMEM_AMBIENT,
                SF64_TAKEOFF_LIGHT_ADDRESS + 0x30U);

    for (arwing_index = 0U; arwing_index < 4U; arwing_index++) {
        if (!state->arwing[arwing_index].draw) {
            continue;
        }
        if (takeoff_emit_arwing(&builder, &root_limb, root_joint_index,
                                &state->arwing[arwing_index], view) != 0) {
            return -1;
        }
    }

    {
        u32 shadow_slot = 10U;
        int shadow_started = 0;
        for (arwing_index = 0U; arwing_index < 4U; arwing_index++) {
            if (!state->arwing[arwing_index].draw ||
                !state->arwing[arwing_index].draw_shadow) {
                continue;
            }
            if (!shadow_started) {
                takeoff_setup_shadow(&builder, title_segment, rdram);
                shadow_started = 1;
            }
            takeoff_append_shadow(&builder, &state->arwing[arwing_index],
                                  state, view, shadow_slot);
            shadow_slot++;
        }
    }

    {
        u32 glow_slot = 6U;
        int glow_started = 0;
        for (arwing_index = 0U; arwing_index < 4U; arwing_index++) {
            if (!state->arwing[arwing_index].draw ||
                !state->arwing[arwing_index].engine_glow) {
                continue;
            }
            if (!glow_started) {
                takeoff_setup_engine_glow(&builder, title_segment, rdram,
                                          SF64_TAKEOFF_GLOW_TEXTURE_OFFSET,
                                          SF64_TAKEOFF_GLOW_VERTEX_OFFSET);
                glow_started = 1;
            }
            takeoff_append_engine_glow(&builder, &state->arwing[arwing_index],
                                       state->eye, view, glow_slot,
                                       SF64_TAKEOFF_GLOW_VERTEX_OFFSET);
            glow_slot++;
        }
    }

    if (state->card_state == 1) {
        takeoff_append_card(&builder, title_segment, rdram);
    }
    takeoff_append_flash(&builder, state, rdram);

    list_append(&builder, SF64_ARWING_G_ENDDL, 0U);

    memset(task, 0, sizeof(*task));
    task->kind = N64PS3_GFX_TASK_DISPLAY_LIST;
    task->dram_base = rdram;
    task->dram_size_bytes = rdram_size;
    task->display_list = rdram + SF64_TAKEOFF_ROOT_OFFSET;
    task->display_list_size_bytes =
        builder.root_command_count * SF64_ARWING_GFX_COMMAND_BYTES;
    task->debug_name = "SF64 opening takeoff";
    return 0;
}

#define SF64_SPACE_GREAT_FOX_OFFSET 0x0007d000U
#define SF64_SPACE_GREAT_FOX_ADDRESS 0x8007d000U
#define SF64_SPACE_CORNERIA_DL 0x06037cf0U
#define SF64_SPACE_GREAT_FOX_DL 0x0e000000U
#define SF64_SPACE_GLOW_TEXTURE_OFFSET 0x0008f000U
/* Sprite vertex bank: one 64-byte quad per glow/explosion so the display list
 * does not replay every quad with the last writer's colour and alpha. */
#define SF64_SPACE_SPRITE_VERTEX_OFFSET 0x00090000U
#define SF64_SPACE_GLOW_VERTEX_OFFSET \
    (SF64_SPACE_SPRITE_VERTEX_OFFSET + 0U * 64U)
#define SF64_SPACE_GLOW_VERTEX2_OFFSET \
    (SF64_SPACE_SPRITE_VERTEX_OFFSET + 19U * 64U)
#define SF64_SPACE_EXPLOSION_VERTEX_OFFSET \
    (SF64_SPACE_SPRITE_VERTEX_OFFSET + 20U * 64U)
#define SF64_SPACE_GLOW_SEGMENT_OFFSET 0x00032178U

static void space_write_light(uint8_t *rdram, uint32_t offset,
                              const Sf64SpaceState *state, int factor)
{
    uint8_t *light = rdram + offset;
    uint8_t *ambient = light + 16;

    memset(light, 0, 16);
    memset(ambient, 0, 16);
    light[0] = (uint8_t)((state->light1_r * factor) / 255);
    light[1] = (uint8_t)((state->light1_g * factor) / 255);
    light[2] = (uint8_t)((state->light1_b * factor) / 255);
    light[8] = (uint8_t)((int32_t)state->light_dir[0] & 0xff);
    light[9] = (uint8_t)((int32_t)state->light_dir[1] & 0xff);
    light[10] = (uint8_t)((int32_t)state->light_dir[2] & 0xff);
    ambient[0] = (uint8_t)((state->ambient_r * factor) / 255);
    ambient[1] = (uint8_t)((state->ambient_g * factor) / 255);
    ambient[2] = (uint8_t)((state->ambient_b * factor) / 255);
}

static void space_append_orb(Sf64ArwingBuilder *builder,
                             Sf64ArwingMatrix base,
                             const float position[3],
                             float scale,
                             unsigned red,
                             unsigned green,
                             unsigned blue,
                             unsigned alpha,
                             uint32_t slot,
                             uint32_t vertex_slot)
{
    Sf64ArwingMatrix matrix;
    uint8_t *vertices = builder->rdram + SF64_SPACE_SPRITE_VERTEX_OFFSET +
                        vertex_slot * 64U;
    uint32_t vertex_offset = SF64_SPACE_SPRITE_VERTEX_OFFSET + vertex_slot * 64U;
    unsigned i;

    memcpy(matrix, base, sizeof(matrix));
    matrix_translate(matrix, position[0], position[1], position[2]);
    matrix_scale(matrix, scale, scale, 1.0f);
    matrix_write_n64(builder->rdram + SF64_TAKEOFF_OBJECT_MATRIX_OFFSET +
                         slot * 64U,
                     matrix);
    list_append(builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                SF64_TAKEOFF_OBJECT_MATRIX_ADDRESS + slot * 64U);
    takeoff_write_vertex3(vertices, -40, 40, 0, 0, 0);
    takeoff_write_vertex3(vertices + 16U, -40, -40, 0, 0, 1024);
    takeoff_write_vertex3(vertices + 32U, 40, -40, 0, 1024, 1024);
    takeoff_write_vertex3(vertices + 48U, 40, 40, 0, 1024, 0);
    for (i = 0; i < 4U; i++) {
        vertices[i * 16U + 12U] = (uint8_t)red;
        vertices[i * 16U + 13U] = (uint8_t)green;
        vertices[i * 16U + 14U] = (uint8_t)blue;
        vertices[i * 16U + 15U] = (uint8_t)alpha;
    }
    list_append(builder, SF64_TAKEOFF_G_VTX | (4U << 10) | (4U * 16U - 1U),
                0x80000000U | vertex_offset);
    list_append(builder, SF64_TAKEOFF_G_TRI1, 0x00000204U);
    list_append(builder, SF64_TAKEOFF_G_TRI1, 0x00000406U);
}

static void space_append_orbs(Sf64ArwingBuilder *builder,
                              Sf64ArwingMatrix base,
                              const Sf64SpaceState *state,
                              uint8_t *rdram)
{
    static const float positions3[4][3] = {
        { -1373.0f, 484.0f, -374.0f },
        { 1373.0f, 484.0f, -374.0f },
        { -1373.0f, -704.0f, -374.0f },
        { 1373.0f, -704.0f, -374.0f }
    };
    static const float positions1[3][3] = {
        { 0.0f, 40.0f, -2278.0f },
        { 295.0f, -92.0f, -1301.0f },
        { -295.0f, -92.0f, -1301.0f }
    };
    static const float blink_scale[8] = {
        0.15f, 0.45f, 0.75f, 1.3f, 0.75f, 0.45f, 0.15f, 0.0f
    };
    static const float s_orb_pos2[8] = {
        -30.0f, -60.0f, -90.0f, -120.0f, -150.0f, -180.0f, -200.0f, -210.0f
    };
    static const float s_orb_scale3[8] = {
        1.5f, 0.87f, 0.83f, 0.75f, 0.7f, 0.6f, 0.4f, 0.1f
    };
    const uint8_t *source = rdram + SF64_TITLE_SEGMENT_RDRAM_OFFSET +
                            SF64_SPACE_GLOW_SEGMENT_OFFSET;
    uint8_t *texture = rdram + SF64_SPACE_GLOW_TEXTURE_OFFSET;
    uint32_t frame = (uint32_t)(state->timer3 < 0 ? 0 : state->timer3);
    uint32_t pixel;
    uint32_t i;

    for (pixel = 0; pixel < 32U * 32U; pixel++) {
        uint8_t ia8 = source[pixel];

        /* Flat white RGB: the source intensity is the alpha shape. Feeding
         * intensity into RGB made zero-intensity texels render opaque black. */
        texture[pixel * 4U] = 255U;
        texture[pixel * 4U + 1U] = 255U;
        texture[pixel * 4U + 2U] = 255U;
        texture[pixel * 4U + 3U] = (uint8_t)((ia8 & 0x0fU) * 17U);
    }

    list_append(builder, SF64_TAKEOFF_G_CLEARGEOMETRYMODE,
                SF64_TAKEOFF_GLOW_CLEAR_BITS);
    list_append(builder, SF64_ARWING_G_TEXTURE, 0xffffffffU);
    list_append(builder, SF64_TAKEOFF_G_SETOTHERMODE_H,
                SF64_TAKEOFF_TEXTURE_FILTER_BILERP);
    list_append(builder, SF64_TAKEOFF_G_SETOTHERMODE_L,
                SF64_TAKEOFF_RENDER_MODE_XLU);
    list_append(builder, SF64_TAKEOFF_G_MOVEWORD_SEGMENT7,
                0x80000000U | SF64_SPACE_GLOW_TEXTURE_OFFSET);
    list_append(builder, SF64_TAKEOFF_G_PIPESYNC, 0U);
    list_append(builder, SF64_TAKEOFF_G_SETTIMG_RGBA32 | 31U, 0x07000000U);
    list_append(builder, SF64_TAKEOFF_G_SETTILE_RGBA32,
                (SF64_TAKEOFF_TEXTURE_TILE_LOAD << 24) |
                    SF64_TAKEOFF_TEXTURE_CLAMP_ST);
    list_append(builder, SF64_TAKEOFF_G_LOADSYNC, 0U);
    list_append(builder, SF64_TAKEOFF_G_LOADBLOCK,
                (SF64_TAKEOFF_TEXTURE_TILE_LOAD << 24) |
                    ((32U * 32U - 1U) << 12));
    list_append(builder, SF64_TAKEOFF_G_PIPESYNC, 0U);
    list_append(builder, SF64_TAKEOFF_G_SETTILE_RGBA32 | (16U << 9),
                SF64_TAKEOFF_TEXTURE_CLAMP_ST);
    list_append(builder, SF64_TAKEOFF_G_SETTILESIZE, (124U << 12) | 124U);

    for (i = 0; i < 4U; i++) {
        float scale = 0.0f;
        unsigned red = i < 2U ? 255U : 32U;
        unsigned blue = i < 2U ? 32U : 255U;

        if (i < 2U) {
            if ((frame & 0x38U) == 0U) {
                scale = blink_scale[frame % 8U];
            }
        } else {
            if (((frame + 32U) & 0x38U) == 0U) {
                scale = blink_scale[frame % 8U];
            }
        }
        if (scale <= 0.0f) {
            continue;
        }
        space_append_orb(builder, base, positions3[i], scale, red, 32U, blue,
                         255U, 2U + i, i);
    }

    for (i = 0; i < 3U; i++) {
        float scale = (frame & 1U) ? 5.2f : 5.0f;
        Sf64ArwingMatrix sub_base;
        uint32_t j;

        space_append_orb(builder, base, positions1[i], scale, 255U, 255U, 0U,
                         48U, 6U + i, 4U + i);
        memcpy(sub_base, base, sizeof(sub_base));
        matrix_translate(sub_base, positions1[i][0], positions1[i][1],
                         positions1[i][2]);
        matrix_scale(sub_base, scale, scale, 1.0f);
        for (j = 0; j < 4U; j++) {
            const float sub_position[3] = { 0.0f, 0.0f, s_orb_pos2[2U * j] };
            space_append_orb(builder, sub_base, sub_position, s_orb_scale3[2U * j],
                             255U, 255U, 0U, 48U, 9U + i * 4U + j,
                             7U + i * 4U + j);
        }
    }
}

static void space_append_explosions(Sf64ArwingBuilder *builder,
                                    Sf64ArwingMatrix corneria,
                                    const Sf64SpaceState *state,
                                    uint8_t *rdram)
{
    const float degrees_to_radians = SF64_ARWING_TWO_PI / 360.0f;
    unsigned i;

    for (i = 0; i < 10U; i++) {
        float scale;
        Sf64ArwingMatrix matrix;
        unsigned alpha;
        unsigned j;
        uint8_t *vertices;
        uint32_t vertex_offset;

        if (state->explosion_state[i] == 255 ||
            state->explosion_scale[i] <= 0.0f ||
            state->corneria_scale == 0.0f) {
            continue;
        }
        vertex_offset = SF64_SPACE_EXPLOSION_VERTEX_OFFSET + i * 64U;
        vertices = rdram + vertex_offset;
        scale = state->explosion_scale[i] / state->corneria_scale;
        memcpy(matrix, corneria, sizeof(matrix));
        matrix_translate(matrix, 0.0f, 0.0f, 5.0f);
        matrix_rotate_z(matrix, state->explosion_rot[i] * degrees_to_radians);
        matrix_translate(matrix, 0.0f, state->explosion_y[i], 0.0f);
        matrix_scale(matrix, scale, scale, scale);
        matrix_write_n64(rdram + SF64_TAKEOFF_OBJECT_MATRIX_OFFSET +
                             (21U + i) * 64U,
                         matrix);
        list_append(builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                    SF64_TAKEOFF_OBJECT_MATRIX_ADDRESS + (21U + i) * 64U);
        takeoff_write_vertex3(vertices, -40, 40, 0, 0, 0);
        takeoff_write_vertex3(vertices + 16U, -40, -40, 0, 0, 1024);
        takeoff_write_vertex3(vertices + 32U, 40, -40, 0, 1024, 1024);
        takeoff_write_vertex3(vertices + 48U, 40, 40, 0, 1024, 0);
        alpha = (unsigned)state->explosion_alpha[i];
        for (j = 0; j < 4U; j++) {
            vertices[j * 16U + 12U] = 255U;
            vertices[j * 16U + 13U] = 200U;
            vertices[j * 16U + 14U] = 200U;
            vertices[j * 16U + 15U] = (uint8_t)alpha;
        }
        list_append(builder, SF64_TAKEOFF_G_VTX | (4U << 10) |
                                 (4U * 16U - 1U),
                    0x80000000U | vertex_offset);
        list_append(builder, SF64_TAKEOFF_G_TRI1, 0x00000204U);
        list_append(builder, SF64_TAKEOFF_G_TRI1, 0x00000406U);
    }
}

int sf64_space_build_task(N64ps3GfxTask *task,
                          uint8_t *rdram,
                          uint32_t rdram_size,
                          const uint8_t *arwing_segment,
                          uint32_t arwing_segment_size,
                          const uint8_t *title_segment,
                          uint32_t title_segment_size,
                          const uint8_t *great_fox_segment,
                          uint32_t great_fox_segment_size,
                          const Sf64SpaceState *state)
{
    const float degrees_to_radians = SF64_ARWING_TWO_PI / 360.0f;
    u32 skeleton[SF64_ARWING_MAX_LIMBS];
    float joint_angles[SF64_ARWING_MAX_LIMBS + 1U][3];
    u32 skeleton_limb_count;
    u32 animation_limb_count;
    u32 root_joint_index;
    Sf64ArwingLimb root_limb;
    Sf64ArwingMatrix projection;
    Sf64ArwingMatrix view;
    Sf64ArwingMatrix object;
    Sf64ArwingMatrix corneria_matrix;
    Sf64ArwingBuilder builder;
    u32 arwing_index;
    int fade_factor;
    int engine_glow_started = 0;

    if (!task || !rdram || !state || !arwing_segment || !title_segment ||
        !great_fox_segment ||
        arwing_segment_size < SF64_ARWING_SEGMENT_SIZE ||
        rdram_size < SF64_ARWING_RDRAM_REQUIRED_SIZE ||
        SF64_TITLE_SEGMENT_RDRAM_OFFSET + title_segment_size > rdram_size ||
        SF64_SPACE_GREAT_FOX_OFFSET + great_fox_segment_size > rdram_size) {
        return -1;
    }

    memset(rdram, 0, rdram_size);
    memcpy(rdram + SF64_ARWING_SEGMENT_RDRAM_OFFSET, arwing_segment,
           SF64_ARWING_SEGMENT_SIZE);
    takeoff_patch_cockpit_glass(rdram);
    memcpy(rdram + SF64_TITLE_SEGMENT_RDRAM_OFFSET, title_segment,
           title_segment_size);
    memcpy(rdram + SF64_SPACE_GREAT_FOX_OFFSET, great_fox_segment,
           great_fox_segment_size);


    if (skeleton_read(arwing_segment, arwing_segment_size,
                      SF64_ARWING_SEGMENT_ID, skeleton,
                      &skeleton_limb_count) != 0 ||
        animation_read(arwing_segment, arwing_segment_size,
                       &animation_limb_count, joint_angles) != 0 ||
        skeleton_limb_count != animation_limb_count ||
        limb_read(&(Sf64ArwingBuilder){ .segment = arwing_segment,
                                        .segment_size = arwing_segment_size,
                                        .segment_id = SF64_ARWING_SEGMENT_ID },
                  skeleton[0], &root_limb) != 0 ||
        find_joint_index(&(Sf64ArwingBuilder){ .skeleton = skeleton },
                         skeleton[0], &root_joint_index) != 0) {
        return -1;
    }

    matrix_perspective(projection, 1.0f);
    matrix_look_at(view, state->eye[0], state->eye[1], state->eye[2],
                   state->at[0], state->at[1], state->at[2],
                   state->up[0], state->up[1], state->up[2]);
    matrix_write_n64(rdram + SF64_TAKEOFF_PROJECTION_OFFSET, projection);
    matrix_write_n64(rdram + SF64_TAKEOFF_VIEW_OFFSET, view);

    fade_factor = 255 - state->fill_alpha;
    if (fade_factor < 0) {
        fade_factor = 0;
    }
    space_write_light(rdram, SF64_TAKEOFF_LIGHT_OFFSET, state, fade_factor);

    memset(&builder, 0, sizeof(builder));
    builder.segment = arwing_segment;
    builder.segment_size = arwing_segment_size;
    builder.segment_id = SF64_ARWING_SEGMENT_ID;
    builder.matrix_offset = SF64_TAKEOFF_MATRIX_OFFSET;
    builder.matrix_address = SF64_TAKEOFF_MATRIX_ADDRESS;
    builder.skeleton = skeleton;
    builder.joint_angles = joint_angles;
    builder.rdram = rdram;

    list_append(&builder, SF64_ARWING_G_MOVEWORD_SEGMENT,
                SF64_ARWING_SEGMENT_ADDRESS);
    list_append(&builder, 0xbc001806U, SF64_TITLE_SEGMENT_RDRAM_OFFSET |
                                           0x80000000U);
    list_append(&builder, 0xbc003806U, SF64_SPACE_GREAT_FOX_ADDRESS);
    list_append(&builder, SF64_ARWING_G_MTX_PROJECTION_LOAD,
                SF64_TAKEOFF_PROJECTION_ADDRESS);
    list_append(&builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                SF64_TAKEOFF_VIEW_ADDRESS);
    list_append(&builder, SF64_ARWING_G_MOVEWORD_NUMLIGHT, 0x00000040U);
    list_append(&builder, SF64_ARWING_G_TEXTURE, 0xffffffffU);
    list_append(&builder, SF64_ARWING_G_SETGEOMETRYMODE,
                SF64_ARWING_G_LIGHTING_ZBUFFER);
    list_append(&builder, SF64_ARWING_G_MOVEMEM_LIGHT,
                SF64_TAKEOFF_LIGHT_ADDRESS);
    list_append(&builder, SF64_ARWING_G_MOVEMEM_AMBIENT,
                SF64_TAKEOFF_LIGHT_ADDRESS + 16U);

    if (state->corneria_draw) {
        float camera_x = state->eye[0] - state->at[0];
        float camera_y = state->eye[1] - state->at[1];
        float camera_z = state->eye[2] - state->at[2];
        float camera_xrot = -atan2f(camera_y,
                                    sqrtf(camera_x * camera_x +
                                          camera_z * camera_z));
        float camera_yrot = atan2f(camera_x, camera_z);

        memcpy(object, view, sizeof(object));
        matrix_translate(object, state->corneria_position[0],
                         state->corneria_position[1],
                         state->eye[2] + 1000.0f);
        matrix_rotate_y(object, camera_yrot);
        matrix_rotate_x(object, camera_xrot);
        matrix_scale(object, state->corneria_scale, state->corneria_scale,
                     state->corneria_scale);
        matrix_write_n64(rdram + SF64_TAKEOFF_OBJECT_MATRIX_OFFSET, object);
        memcpy(corneria_matrix, object, sizeof(corneria_matrix));
        /* The planet is an unlit texture in the original render setup. */
        list_append(&builder, SF64_TAKEOFF_G_CLEARGEOMETRYMODE, 0x00020000U);
        list_append(&builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                    SF64_TAKEOFF_OBJECT_MATRIX_ADDRESS);
        list_append(&builder, SF64_ARWING_G_DL, SF64_SPACE_CORNERIA_DL);
        list_append(&builder, SF64_ARWING_G_SETGEOMETRYMODE,
                    SF64_ARWING_G_LIGHTING_ZBUFFER);
    }

    memcpy(object, view, sizeof(object));
    matrix_translate(object, state->great_fox_position[0],
                     state->great_fox_position[1],
                     state->great_fox_position[2]);
    matrix_rotate_z(object, state->great_fox_rotation[2] * degrees_to_radians);
    matrix_rotate_x(object, state->great_fox_rotation[0] * degrees_to_radians);
    matrix_rotate_y(object, state->great_fox_rotation[1] * degrees_to_radians);
    matrix_scale(object, state->great_fox_scale, state->great_fox_scale,
                 state->great_fox_scale);
    matrix_write_n64(rdram + SF64_TAKEOFF_OBJECT_MATRIX_OFFSET + 64U, object);
    list_append(&builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                SF64_TAKEOFF_OBJECT_MATRIX_ADDRESS + 64U);
    list_append(&builder, SF64_ARWING_G_DL, SF64_SPACE_GREAT_FOX_DL);

    for (arwing_index = 0U; arwing_index < 4U; arwing_index++) {
        if (!state->arwing[arwing_index].draw) {
            continue;
        }
        if (takeoff_emit_arwing(&builder, &root_limb, root_joint_index,
                                &state->arwing[arwing_index], view) != 0) {
            return -1;
        }
        if (state->arwing[arwing_index].engine_glow) {
            if (!engine_glow_started) {
                takeoff_setup_engine_glow(&builder, title_segment, rdram,
                                          SF64_SPACE_GLOW_TEXTURE_OFFSET,
                                          SF64_SPACE_GLOW_VERTEX2_OFFSET);
                engine_glow_started = 1;
            }
            takeoff_append_engine_glow(&builder, &state->arwing[arwing_index],
                                       state->eye, view,
                                       40U + arwing_index,
                                       SF64_SPACE_GLOW_VERTEX2_OFFSET);
        }
    }

    space_append_orbs(&builder, object, state, rdram);
    if (state->corneria_draw) {
        space_append_explosions(&builder, corneria_matrix, state, rdram);
    }

    list_append(&builder, SF64_ARWING_G_ENDDL, 0U);

    memset(task, 0, sizeof(*task));
    task->kind = N64PS3_GFX_TASK_DISPLAY_LIST;
    task->dram_base = rdram;
    task->dram_size_bytes = rdram_size;
    task->display_list = rdram + SF64_TAKEOFF_ROOT_OFFSET;
    task->display_list_size_bytes =
        builder.root_command_count * SF64_ARWING_GFX_COMMAND_BYTES;
    task->debug_name = "SF64 opening space";
    return 0;
}

#define SF64_TEAM_CARD_TEXTURE_OFFSET 0x00085000U
#define SF64_TEAM_CARD_VERTEX_OFFSET 0x00087400U
#define SF64_TEAM_CARD_ORTHO_OFFSET 0x00008600U
#define SF64_TEAM_CARD_IDENTITY_OFFSET 0x00008640U
#define SF64_TEAM_CARD_ORTHO_ADDRESS 0x80008600U
#define SF64_TEAM_CARD_IDENTITY_ADDRESS 0x80008640U

static void team_card_ortho_matrix(Sf64ArwingMatrix matrix)
{
    float scale_x;
    float scale_y;

    memset(matrix, 0, sizeof(Sf64ArwingMatrix));
    sf64_display_fit_4_3(sf64_display_aspect(), &scale_x, &scale_y);
    matrix[0][0] = (2.0f / 320.0f) * scale_x;
    matrix[1][1] = (-2.0f / 240.0f) * scale_y;
    matrix[2][2] = 1.0f;
    matrix[3][0] = -scale_x;
    matrix[3][1] = scale_y;
    matrix[3][3] = 1.0f;
}

static int team_running_append_card(N64ps3GfxTask *task, const u8 *title,
                                    u32 title_size, int team_index,
                                    u8 *rdram)
{
    static const uint32_t offsets[4] = {
        0x0000f940U, 0x00010090U, 0x000106b0U, 0x00010fa0U
    };
    static const uint32_t widths[4] = { 144U, 120U, 176U, 176U };
    static const float x_positions[4] = { 90.0f, 100.0f, 75.0f, 75.0f };
    Sf64ArwingMatrix ortho;
    Sf64ArwingMatrix identity;
    Sf64ArwingBuilder builder;
    uint8_t *texture = rdram + SF64_TEAM_CARD_TEXTURE_OFFSET;
    uint8_t *vertices = rdram + SF64_TEAM_CARD_VERTEX_OFFSET;
    uint32_t width;
    uint32_t row_words;
    uint32_t pixel;
    uint32_t end_index;
    int16_t x0;
    int16_t x1;

    if (team_index < 0 || team_index >= 4) {
        return 0;
    }
    width = widths[team_index];
    if (offsets[team_index] > title_size ||
        width * 13U > title_size - offsets[team_index]) {
        return -1;
    }

    for (pixel = 0; pixel < width * 13U; pixel++) {
        uint8_t ia8 = title[offsets[team_index] + pixel];
        uint8_t intensity = (uint8_t)((ia8 >> 4) * 17U);
        uint8_t alpha = (uint8_t)((ia8 & 0x0fU) * 17U);

        if (alpha <= 32U) {
            alpha = 0U;
        } else {
            alpha = (uint8_t)((((uint32_t)(alpha - 32U) * 255U) + 111U) / 223U);
        }
        texture[pixel * 4U] = intensity;
        texture[pixel * 4U + 1U] = intensity;
        texture[pixel * 4U + 2U] = intensity;
        texture[pixel * 4U + 3U] = alpha;
    }

    team_card_ortho_matrix(ortho);
    matrix_identity(identity);
    matrix_write_n64(rdram + SF64_TEAM_CARD_ORTHO_OFFSET, ortho);
    matrix_write_n64(rdram + SF64_TEAM_CARD_IDENTITY_OFFSET, identity);

    end_index = task->display_list_size_bytes / SF64_ARWING_GFX_COMMAND_BYTES;
    if (end_index == 0U) {
        return -1;
    }
    memset(&builder, 0, sizeof(builder));
    builder.rdram = rdram;
    builder.root_command_count = end_index - 1U;

    list_append(&builder, SF64_ARWING_G_MTX_PROJECTION_LOAD,
                SF64_TEAM_CARD_ORTHO_ADDRESS);
    list_append(&builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                SF64_TEAM_CARD_IDENTITY_ADDRESS);
    list_append(&builder, SF64_TAKEOFF_G_CLEARGEOMETRYMODE,
                SF64_TAKEOFF_CLEAR_GEOMETRY_BITS);
    list_append(&builder, SF64_ARWING_G_TEXTURE, 0xffffffffU);
    list_append(&builder, SF64_TAKEOFF_G_SETOTHERMODE_H, 0x00000000U);
    list_append(&builder, SF64_TAKEOFF_G_SETOTHERMODE_L,
                SF64_TAKEOFF_RENDER_MODE_XLU);
    list_append(&builder, SF64_TAKEOFF_G_MOVEWORD_SEGMENT7,
                0x80000000U | SF64_TEAM_CARD_TEXTURE_OFFSET);
    list_append(&builder, SF64_TAKEOFF_G_PIPESYNC, 0U);
    list_append(&builder, SF64_TAKEOFF_G_SETTIMG_RGBA32 | (width - 1U),
                0x07000000U);
    list_append(&builder, SF64_TAKEOFF_G_SETTILE_RGBA32,
                (SF64_TAKEOFF_TEXTURE_TILE_LOAD << 24) |
                    SF64_TAKEOFF_TEXTURE_CLAMP_ST);
    list_append(&builder, SF64_TAKEOFF_G_LOADSYNC, 0U);
    list_append(&builder, SF64_TAKEOFF_G_LOADBLOCK,
                (SF64_TAKEOFF_TEXTURE_TILE_LOAD << 24) |
                    ((width * 13U - 1U) << 12));
    list_append(&builder, SF64_TAKEOFF_G_PIPESYNC, 0U);
    row_words = (width * 4U + 7U) / 8U;
    list_append(&builder, SF64_TAKEOFF_G_SETTILE_RGBA32 | (row_words << 9),
                SF64_TAKEOFF_TEXTURE_CLAMP_ST);
    list_append(&builder, SF64_TAKEOFF_G_SETTILESIZE,
                (((width - 1U) * 4U) << 12) | ((13U - 1U) * 4U));

    x0 = (int16_t)x_positions[team_index];
    x1 = (int16_t)(x_positions[team_index] + (float)width);
    takeoff_write_vertex(vertices, x0, 210, 0, 0);
    takeoff_write_vertex(vertices + 16U, x1, 210, (int16_t)(width * 32U), 0);
    takeoff_write_vertex(vertices + 32U, x1, 223, (int16_t)(width * 32U),
                         13 * 32);
    takeoff_write_vertex(vertices + 48U, x0, 223, 0, 13 * 32);

    list_append(&builder, SF64_TAKEOFF_G_VTX | (4U << 10) | (4U * 16U - 1U),
                0x80000000U | SF64_TEAM_CARD_VERTEX_OFFSET);
    list_append(&builder, SF64_TAKEOFF_G_TRI1, 0x00000204U);
    list_append(&builder, SF64_TAKEOFF_G_TRI1, 0x00000406U);
    list_append(&builder, SF64_ARWING_G_ENDDL, 0U);

    task->display_list_size_bytes =
        builder.root_command_count * SF64_ARWING_GFX_COMMAND_BYTES;
    return 0;
}

#define SF64_TEAM_SHADOW_TEXTURE_OFFSET 0x00087600U
#define SF64_TEAM_SHADOW_VERTEX_OFFSET 0x00088700U
#define SF64_TEAM_SHADOW_MATRIX_OFFSET 0x0008f000U
#define SF64_TEAM_SHADOW_MATRIX_ADDRESS 0x8008f000U
#define SF64_TEAM_SHADOW_SEGMENT_OFFSET 0x000410f8U

static int team_running_append_shadows(N64ps3GfxTask *task,
                                       const Sf64TitleTeamModel models[4],
                                       const float eye[3], const float at[3],
                                       const float up[3], float team_z_offset,
                                       u8 *rdram)
{
    const uint8_t *source = rdram + SF64_TITLE_SEGMENT_RDRAM_OFFSET +
                            SF64_TEAM_SHADOW_SEGMENT_OFFSET;
    uint8_t *texture = rdram + SF64_TEAM_SHADOW_TEXTURE_OFFSET;
    uint8_t *vertices = rdram + SF64_TEAM_SHADOW_VERTEX_OFFSET;
    Sf64ArwingMatrix view;
    Sf64ArwingMatrix matrix;
    Sf64ArwingBuilder builder;
    uint32_t pixel;
    uint32_t end_index;
    uint32_t i;

    for (pixel = 0; pixel < 32U * 32U; pixel++) {
        uint16_t value = (uint16_t)(((uint16_t)source[pixel * 2U] << 8) |
                                    source[pixel * 2U + 1U]);
        uint8_t red = (uint8_t)((value >> 11) & 31U);
        uint8_t green = (uint8_t)((value >> 6) & 31U);
        uint8_t blue = (uint8_t)((value >> 1) & 31U);

        texture[pixel * 4U] = (uint8_t)((red << 3) | (red >> 2));
        texture[pixel * 4U + 1U] = (uint8_t)((green << 3) | (green >> 2));
        texture[pixel * 4U + 2U] = (uint8_t)((blue << 3) | (blue >> 2));
        texture[pixel * 4U + 3U] = (value & 1U) ? 128U : 0U;
    }

    takeoff_write_vertex3(vertices, -60, 0, 40, -1024, 1536);
    takeoff_write_vertex3(vertices + 16U, 60, 0, 40, 2048, 1536);
    takeoff_write_vertex3(vertices + 32U, 0, 0, -60, 512, -1024);
    for (i = 0; i < 3U; i++) {
        vertices[i * 16U + 15U] = 255U;
    }

    end_index = task->display_list_size_bytes / SF64_ARWING_GFX_COMMAND_BYTES;
    if (end_index == 0U) {
        return -1;
    }
    memset(&builder, 0, sizeof(builder));
    builder.rdram = rdram;
    builder.root_command_count = end_index - 1U;

    matrix_look_at(view, eye[0], eye[1], eye[2], at[0], at[1], at[2], up[0],
                   up[1], up[2]);

    list_append(&builder, SF64_TAKEOFF_G_CLEARGEOMETRYMODE, 0x00020000U);
    list_append(&builder, SF64_ARWING_G_TEXTURE, 0xffffffffU);
    list_append(&builder, SF64_TAKEOFF_G_SETOTHERMODE_H,
                SF64_TAKEOFF_TEXTURE_FILTER_BILERP);
    list_append(&builder, SF64_TAKEOFF_G_SETOTHERMODE_L,
                SF64_TAKEOFF_RENDER_MODE_XLU);
    list_append(&builder, SF64_TAKEOFF_G_MOVEWORD_SEGMENT7,
                0x80000000U | SF64_TEAM_SHADOW_TEXTURE_OFFSET);
    list_append(&builder, SF64_TAKEOFF_G_PIPESYNC, 0U);
    list_append(&builder, SF64_TAKEOFF_G_SETTIMG_RGBA32 | 31U, 0x07000000U);
    list_append(&builder, SF64_TAKEOFF_G_SETTILE_RGBA32,
                (SF64_TAKEOFF_TEXTURE_TILE_LOAD << 24) |
                    SF64_TAKEOFF_TEXTURE_CLAMP_ST);
    list_append(&builder, SF64_TAKEOFF_G_LOADSYNC, 0U);
    list_append(&builder, SF64_TAKEOFF_G_LOADBLOCK,
                (SF64_TAKEOFF_TEXTURE_TILE_LOAD << 24) |
                    ((32U * 32U - 1U) << 12));
    list_append(&builder, SF64_TAKEOFF_G_PIPESYNC, 0U);
    list_append(&builder,
                SF64_TAKEOFF_G_SETTILE_RGBA32 |
                    (((32U * 4U + 7U) / 8U) << 9),
                SF64_TAKEOFF_TEXTURE_CLAMP_ST);
    list_append(&builder, SF64_TAKEOFF_G_SETTILESIZE, (124U << 12) | 124U);

    for (i = 0; i < 4U; i++) {
        memcpy(matrix, view, sizeof(matrix));
        matrix_translate(matrix, models[i].position[0] - 5.0f, 5.0f,
                         models[i].position[2] + 10.0f + team_z_offset);
        matrix_scale(matrix, 3.0f, 3.0f, 3.0f);
        matrix_write_n64(rdram + SF64_TEAM_SHADOW_MATRIX_OFFSET + i * 64U,
                         matrix);
        list_append(&builder, SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                    SF64_TEAM_SHADOW_MATRIX_ADDRESS + i * 64U);
        list_append(&builder,
                    SF64_TAKEOFF_G_VTX | (3U << 10) | (3U * 16U - 1U),
                    0x80000000U | SF64_TEAM_SHADOW_VERTEX_OFFSET);
        list_append(&builder, SF64_TAKEOFF_G_TRI1, 0x00000204U);
    }

    list_append(&builder, SF64_ARWING_G_ENDDL, 0U);
    task->display_list_size_bytes =
        builder.root_command_count * SF64_ARWING_GFX_COMMAND_BYTES;
    return 0;
}

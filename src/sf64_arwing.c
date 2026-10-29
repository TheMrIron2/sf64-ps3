#include "sf64_ps3/arwing.h"

#include <math.h>
#include <string.h>

#define SF64_ARWING_SKELETON_OFFSET 0x00016610U
/* GSTATE_TITLE uses aAwWingsClosedAnim in Display_Arwing_Skel. */
#define SF64_ARWING_ANIMATION_OFFSET 0x00015c28U
#define SF64_ARWING_MAX_LIMBS 32U
#define SF64_ARWING_ROOT_LIST_OFFSET 0x00000100U
#define SF64_ARWING_MATRIX_OFFSET 0x00007000U
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

    if (segment != SF64_ARWING_SEGMENT_ID ||
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

static void matrix_perspective(Sf64ArwingMatrix matrix)
{
    const float degrees_to_radians = SF64_ARWING_TWO_PI / 360.0f;
    const float field_of_view_y = 45.0f;
    const float aspect = 4.0f / 3.0f;
    const float near_plane = 10.0f;
    const float far_plane = 12800.0f;
    float half_fov = field_of_view_y * degrees_to_radians * 0.5f;
    float cotangent = cosf(half_fov) / sinf(half_fov);

    memset(matrix, 0, sizeof(Sf64ArwingMatrix));
    matrix[0][0] = cotangent / aspect;
    matrix[1][1] = cotangent;
    matrix[2][2] = (near_plane + far_plane) / (near_plane - far_plane);
    matrix[2][3] = -1.0f;
    matrix[3][2] = (2.0f * near_plane * far_plane) / (near_plane - far_plane);
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
        u32 matrix_offset = SF64_ARWING_MATRIX_OFFSET +
                            builder->display_list_count * SF64_ARWING_G_MTX_BYTES;
        u8 *matrix_bytes;

        if (builder->display_list_count >= SF64_ARWING_MAX_LIMBS ||
            segment_offset(builder, limb.display_list, 8U, &joint_index) != 0) {
            return -1;
        }
        matrix_multiply(final_matrix, *world_matrix, local_matrix);
        matrix_bytes = builder->rdram + matrix_offset;
        matrix_write_n64(matrix_bytes, final_matrix);
        list_append(builder,
                    SF64_ARWING_G_MTX_MODELVIEW_LOAD,
                    SF64_ARWING_MATRIX_ADDRESS +
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
        if ((address >> 24) != SF64_ARWING_SEGMENT_ID ||
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

    if (skeleton_read(segment, segment_size, skeleton, &skeleton_limb_count) != 0 ||
        animation_read(segment, segment_size, &animation_limb_count, joint_angles) != 0 ||
        skeleton_limb_count != animation_limb_count ||
        limb_read(&(Sf64ArwingBuilder){ .segment = segment,
                                        .segment_size = segment_size },
                  skeleton[0], &root_limb) != 0 ||
        find_joint_index(&(Sf64ArwingBuilder){ .skeleton = skeleton },
                         skeleton[0], &root_joint_index) != 0) {
        return -1;
    }

    matrix_look_at(view_matrix,
                   0.0f, 180.0f, 430.0f,
                   0.0f, 180.0f, 0.0f,
                   0.0f, 1.0f, 0.0f);
    matrix_perspective(projection_matrix);
    matrix_write_n64(rdram + SF64_ARWING_PROJECTION_OFFSET, projection_matrix);
    matrix_write_n64(rdram + SF64_ARWING_VIEW_MATRIX_OFFSET, view_matrix);
    write_light_data(rdram);
    memset(&builder, 0, sizeof(builder));
    builder.segment = segment;
    builder.segment_size = segment_size;
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

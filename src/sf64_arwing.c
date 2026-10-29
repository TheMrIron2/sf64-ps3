#include "sf64_ps3/arwing.h"

#include <math.h>
#include <string.h>

#define SF64_ARWING_SKELETON_OFFSET 0x00016610U
#define SF64_ARWING_ANIMATION_OFFSET 0x00015af4U
#define SF64_ARWING_MAX_LIMBS 32U
#define SF64_ARWING_ROOT_LIST_OFFSET 0x00000100U
#define SF64_ARWING_LIGHT_OFFSET 0x00000800U
#define SF64_ARWING_MATRIX_OFFSET 0x00001000U
#define SF64_ARWING_SEGMENT_RDRAM_OFFSET 0x00010000U
#define SF64_ARWING_ROOT_LIST_ADDRESS 0x80000100U
#define SF64_ARWING_LIGHT_ADDRESS 0x80000800U
#define SF64_ARWING_MATRIX_ADDRESS 0x80001000U
#define SF64_ARWING_SEGMENT_ADDRESS 0x80010000U
#define SF64_ARWING_G_MTX_MODELVIEW_LOAD 0x01020040U
#define SF64_ARWING_G_DL 0x06000000U
#define SF64_ARWING_G_ENDDL 0xb8000000U
#define SF64_ARWING_G_MOVEWORD_SEGMENT 0xbc000c06U
#define SF64_ARWING_G_MOVEWORD_NUMLIGHT 0xbc000002U
#define SF64_ARWING_G_MOVEMEM_LIGHT 0x03860010U
#define SF64_ARWING_G_MOVEMEM_AMBIENT 0x03880010U
#define SF64_ARWING_G_TEXTURE 0xd7000001U
#define SF64_ARWING_G_SETGEOMETRYMODE 0xb7000000U
#define SF64_ARWING_G_LIGHTING_ZBUFFER 0x00020001U
#define SF64_ARWING_G_MTX_BYTES 64U
#define SF64_ARWING_GFX_COMMAND_BYTES 8U
#define SF64_ARWING_DLIST_COMMANDS_PER_LIMB 2U
#define SF64_ARWING_LIGHT_DIRECTIONAL_BYTES 16U
#define SF64_ARWING_LIGHT_AMBIENT_BYTES 16U
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
    u32 visited[SF64_ARWING_MAX_LIMBS];
    u32 visited_count;
} Sf64ArwingBuilder;

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

static void build_title_world_matrix(Sf64ArwingMatrix matrix)
{
    const float degrees_to_radians = SF64_ARWING_TWO_PI / 360.0f;

    matrix_identity(matrix);
    /* The game camera is not in the harness yet; center Fox's title transform. */
    matrix_translate(matrix, 80.0f, -220.0f, 360.0f);
    matrix_translate(matrix, -80.0f, 220.0f, -360.0f);
    matrix_scale(matrix, 4.0f, 4.0f, 4.0f);
    matrix_rotate_z(matrix, -15.0f * degrees_to_radians);
    matrix_rotate_x(matrix, 35.0f * degrees_to_radians);
    matrix_rotate_y(matrix, -90.0f * degrees_to_radians);
    /* Orthographic fallback has no title camera, so zoom to fit the frame. */
    matrix_scale(matrix, 0.75f, 0.75f, 0.75f);
}

static void write_light_data(u8 *rdram)
{
    u8 *directional = rdram + SF64_ARWING_LIGHT_OFFSET;
    u8 *ambient = directional + SF64_ARWING_LIGHT_DIRECTIONAL_BYTES;

    memset(directional, 0, SF64_ARWING_LIGHT_DIRECTIONAL_BYTES);
    directional[0] = 150U;
    directional[1] = 150U;
    directional[2] = 150U;
    directional[8] = 0U;
    directional[9] = 0U;
    directional[10] = 127U;

    memset(ambient, 0, SF64_ARWING_LIGHT_AMBIENT_BYTES);
    ambient[0] = 20U;
    ambient[1] = 20U;
    ambient[2] = 20U;
}

int sf64_arwing_build_title_task(const u8 *segment,
                                 u32 segment_size,
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
    memset(rdram, 0, rdram_size);
    memcpy(rdram + SF64_ARWING_SEGMENT_RDRAM_OFFSET,
           segment, SF64_ARWING_SEGMENT_SIZE);

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
    build_title_world_matrix(world_matrix);

    list_append(&builder, SF64_ARWING_G_MOVEWORD_SEGMENT,
                SF64_ARWING_SEGMENT_ADDRESS);
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
    if (SF64_ARWING_ROOT_LIST_OFFSET + command_bytes > SF64_ARWING_LIGHT_OFFSET ||
        SF64_ARWING_MATRIX_OFFSET + builder.display_list_count * SF64_ARWING_G_MTX_BYTES >
            SF64_ARWING_SEGMENT_RDRAM_OFFSET) {
        return -1;
    }

    memset(task, 0, sizeof(*task));
    task->kind = N64PS3_GFX_TASK_DISPLAY_LIST;
    task->dram_base = rdram;
    task->dram_size_bytes = rdram_size;
    task->display_list = rdram + SF64_ARWING_ROOT_LIST_OFFSET;
    task->display_list_size_bytes = command_bytes;
    task->debug_name = "SF64 US Rev 1 Arwing title pose";

    if (info) {
        info->animation_frame = 0U;
        info->skeleton_limb_count = skeleton_limb_count;
        info->display_list_count = builder.display_list_count;
        info->generated_command_count = builder.root_command_count;
    }
    return 0;
}

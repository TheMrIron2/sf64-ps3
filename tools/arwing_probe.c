#include "n64ps3/gfx.h"
#include "n64ps3/gfx_f3dex1.h"
#include "n64ps3/pi.h"
#include "sf64_ps3/arwing.h"
#include "sf64_ps3/title_starfield.h"

#include <stdio.h>
#include <string.h>

#define SF64_US_REV1_DMA_TABLE 0x000de480U
#define SF64_AST_ARWING_DMA_INDEX 9U
#define SF64_AST_ARWING_ROM_START 0x008bfc00U
#define SF64_AST_ARWING_ROM_END 0x008d99f0U
#define SF64_AST_ARWING_BODY_DL_OFFSET 0x00009b60U
#define SF64_AST_ARWING_BODY_DL_ROM_OFFSET \
    (SF64_AST_ARWING_ROM_START + SF64_AST_ARWING_BODY_DL_OFFSET)
#define SF64_AST_ARWING_SKELETON_OFFSET 0x00016610U
#define SF64_AST_ARWING_SEGMENT_BYTES \
    (SF64_AST_ARWING_ROM_END - SF64_AST_ARWING_ROM_START)
#define SF64_ARWING_PROBE_MAX_TEXTURED_TRIANGLES 512U

static const u8 s_expected_body_header[] = {
    0xfd, 0x10, 0x00, 0x00, 0x03, 0x00, 0xac, 0x48
};

static const u8 s_expected_skeleton_root[] = {
    0x03, 0x01, 0x65, 0xf0
};

static u8 s_arwing_segment[SF64_AST_ARWING_SEGMENT_BYTES];
static u8 s_rdram[SF64_ARWING_RDRAM_REQUIRED_SIZE];
static u8 s_title_starfield[SF64_TITLE_STARFIELD_RDRAM_SIZE];
static u8 s_next_title_starfield[SF64_TITLE_STARFIELD_RDRAM_SIZE];

static u32 read_be32(const u8 *bytes)
{
    return ((u32)bytes[0] << 24) |
           ((u32)bytes[1] << 16) |
           ((u32)bytes[2] << 8) |
           (u32)bytes[3];
}

static const char *opcode_name(u32 opcode)
{
    switch (opcode) {
    case 0xb4: return "RDPHALF_1";
    case 0xd7: return "TEXTURE";
    case 0xe2: return "SETOTHERMODE_L";
    case 0xe3: return "SETOTHERMODE_H";
    case 0xe6: return "LOADSYNC";
    case 0xe7: return "PIPESYNC";
    case 0xe8: return "TILESYNC";
    case 0xf0: return "LOADTLUT";
    case 0xf2: return "SETTILESIZE";
    case 0xf3: return "LOADBLOCK";
    case 0xf4: return "LOADTILE";
    case 0xf5: return "SETTILE";
    case 0xf6: return "FILLRECT";
    case 0xf7: return "FILL";
    case 0xf8: return "SETFOGCOLOR";
    case 0xf9: return "SETBLENDCOLOR";
    case 0xfa: return "SETPRIMCOLOR";
    case 0xfb: return "SETENVCOLOR";
    case 0xfc: return "SETCOMBINE";
    case 0xfd: return "SETTIMG";
    case 0xfe: return "SETZ";
    default: return "RDP/other";
    }
}

typedef struct Sf64ArwingProbe {
    u32 triangles;
    u32 vertices_in_clip;
    u32 textured_triangles;
    u32 unique_textures;
    u32 texture_state_hash;
    u32 texture_shade_hash;
    u8 texture_shade[SF64_ARWING_PROBE_MAX_TEXTURED_TRIANGLES][3][4];
    u32 nonzero_tile_origins;
    u32 nonzero_tile_shifts;
    float first_textured_clip[4];
    s16 first_textured_st[6];
    const u8 *texture_sources[64];
    u16 texture_widths[64];
    u16 texture_heights[64];
    u16 first_texture_uls;
    u16 first_texture_ult;
    u8 first_texture_mask_s;
    u8 first_texture_mask_t;
    u8 first_texture_shift_s;
    u8 first_texture_shift_t;
    u8 first_texture_wrap_s;
    u8 first_texture_wrap_t;
    u32 texture_hashes[64];
    float minimum[3];
    float maximum[3];
} Sf64ArwingProbe;

static int record_triangle(void *user_data,
                           const N64ps3GfxF3dex1Vertex *a,
                           const N64ps3GfxF3dex1Vertex *b,
                           const N64ps3GfxF3dex1Vertex *c)
{
    Sf64ArwingProbe *probe = (Sf64ArwingProbe *)user_data;
    const N64ps3GfxF3dex1Vertex *vertices[3] = { a, b, c };
    u32 vertex_index;

    probe->triangles++;
    if (a->texture_enabled && a->texture_data &&
        a->texture_bytes_per_texel == 2U) {
        u32 texture_index;
        u32 vertex_index;

        if (probe->textured_triangles == 0U) {
            probe->texture_state_hash = 2166136261U;
            probe->texture_shade_hash = 2166136261U;
        }
        for (vertex_index = 0; vertex_index < 3U; vertex_index++) {
            const N64ps3GfxF3dex1Vertex *vertex = vertices[vertex_index];
            const u16 values[] = {
                (u16)vertex->s, (u16)vertex->t,
                vertex->texture_scale_s, vertex->texture_scale_t,
                vertex->texture_uls, vertex->texture_ult,
                vertex->texture_width, vertex->texture_height
            };
            u32 value_index;

            for (value_index = 0;
                 value_index < sizeof(values) / sizeof(values[0]);
                 value_index++) {
                probe->texture_state_hash =
                    (probe->texture_state_hash ^ (u8)values[value_index]) *
                    16777619U;
                probe->texture_state_hash =
                    (probe->texture_state_hash ^ (u8)(values[value_index] >> 8)) *
                    16777619U;
            }
            probe->texture_state_hash =
                (probe->texture_state_hash ^ vertex->texture_mask_s) * 16777619U;
            probe->texture_state_hash =
                (probe->texture_state_hash ^ vertex->texture_mask_t) * 16777619U;
            probe->texture_state_hash =
                (probe->texture_state_hash ^ vertex->texture_shift_s) * 16777619U;
            probe->texture_state_hash =
                (probe->texture_state_hash ^ vertex->texture_shift_t) * 16777619U;
            probe->texture_state_hash =
                (probe->texture_state_hash ^ vertex->texture_wrap_s) * 16777619U;
            probe->texture_state_hash =
                (probe->texture_state_hash ^ vertex->texture_wrap_t) * 16777619U;
            probe->texture_shade_hash =
                (probe->texture_shade_hash ^ vertex->color[0]) * 16777619U;
            probe->texture_shade_hash =
                (probe->texture_shade_hash ^ vertex->color[1]) * 16777619U;
            probe->texture_shade_hash =
                (probe->texture_shade_hash ^ vertex->color[2]) * 16777619U;
            probe->texture_shade_hash =
                (probe->texture_shade_hash ^ vertex->color[3]) * 16777619U;
            if (probe->textured_triangles <
                SF64_ARWING_PROBE_MAX_TEXTURED_TRIANGLES) {
                memcpy(probe->texture_shade[probe->textured_triangles]
                                               [vertex_index],
                       vertex->color, 4U);
            }
        }
        if (a->texture_uls != 0U || a->texture_ult != 0U) {
            probe->nonzero_tile_origins++;
        }
        if (a->texture_shift_s != 0U || a->texture_shift_t != 0U) {
            probe->nonzero_tile_shifts++;
        }
        if (probe->textured_triangles == 0U) {
            probe->first_textured_clip[0] = a->clip_x;
            probe->first_textured_clip[1] = a->clip_y;
            probe->first_textured_clip[2] = a->clip_z;
            probe->first_textured_clip[3] = a->clip_w;
            probe->first_textured_st[0] = a->s;
            probe->first_textured_st[1] = a->t;
            probe->first_textured_st[2] = b->s;
            probe->first_textured_st[3] = b->t;
            probe->first_textured_st[4] = c->s;
            probe->first_textured_st[5] = c->t;
            probe->first_texture_uls = a->texture_uls;
            probe->first_texture_ult = a->texture_ult;
            probe->first_texture_mask_s = a->texture_mask_s;
            probe->first_texture_mask_t = a->texture_mask_t;
            probe->first_texture_shift_s = a->texture_shift_s;
            probe->first_texture_shift_t = a->texture_shift_t;
            probe->first_texture_wrap_s = a->texture_wrap_s;
            probe->first_texture_wrap_t = a->texture_wrap_t;
        }
        probe->textured_triangles++;
        for (texture_index = 0; texture_index < probe->unique_textures;
             texture_index++) {
            if (probe->texture_sources[texture_index] == a->texture_data &&
                probe->texture_widths[texture_index] == a->texture_width &&
                probe->texture_heights[texture_index] == a->texture_height) {
                break;
            }
        }
        if (texture_index == probe->unique_textures &&
            probe->unique_textures < 64U) {
            const u8 *bytes = a->texture_data;
            u32 byte_count = (u32)a->texture_width * a->texture_height * 2U;
            u32 hash = 2166136261U;
            u32 byte_index;

            for (byte_index = 0; byte_index < byte_count; byte_index++) {
                hash = (hash ^ bytes[byte_index]) * 16777619U;
            }
            probe->texture_sources[texture_index] = a->texture_data;
            probe->texture_widths[texture_index] = a->texture_width;
            probe->texture_heights[texture_index] = a->texture_height;
            probe->texture_hashes[texture_index] = hash;
            probe->unique_textures++;
        }
    }
    for (vertex_index = 0; vertex_index < 3U; vertex_index++) {
        float coordinates[3];
        float inverse_w = 0.0f;
        u32 axis;

        if (vertices[vertex_index]->clip_valid != 0U &&
            vertices[vertex_index]->clip_w != 0.0f) {
            inverse_w = 1.0f / vertices[vertex_index]->clip_w;
            coordinates[0] = vertices[vertex_index]->clip_x * inverse_w;
            coordinates[1] = vertices[vertex_index]->clip_y * inverse_w;
            coordinates[2] = vertices[vertex_index]->clip_z * inverse_w;
        } else {
            coordinates[0] = 0.0f;
            coordinates[1] = 0.0f;
            coordinates[2] = 0.0f;
        }
        if (vertices[vertex_index]->clip_valid != 0U &&
            coordinates[0] >= -1.0f && coordinates[0] <= 1.0f &&
            coordinates[1] >= -1.0f && coordinates[1] <= 1.0f &&
            coordinates[2] >= -1.0f && coordinates[2] <= 1.0f) {
            probe->vertices_in_clip++;
        }
        for (axis = 0; axis < 3U; axis++) {
            if (probe->triangles == 1U && vertex_index == 0U) {
                probe->minimum[axis] = coordinates[axis];
                probe->maximum[axis] = coordinates[axis];
            } else {
                if (coordinates[axis] < probe->minimum[axis]) {
                    probe->minimum[axis] = coordinates[axis];
                }
                if (coordinates[axis] > probe->maximum[axis]) {
                    probe->maximum[axis] = coordinates[axis];
                }
            }
        }
    }
    if (probe->triangles <= 3U) {
        printf("triangle %u raw=(%d,%d,%d),(%d,%d,%d),(%d,%d,%d) "
               "ndc=(%.3f,%.3f,%.3f),(%.3f,%.3f,%.3f),(%.3f,%.3f,%.3f)\n",
               probe->triangles,
               a->x, a->y, a->z, b->x, b->y, b->z, c->x, c->y, c->z,
               a->clip_x / a->clip_w, a->clip_y / a->clip_w,
               a->clip_z / a->clip_w,
               b->clip_x / b->clip_w, b->clip_y / b->clip_w,
               b->clip_z / b->clip_w,
               c->clip_x / c->clip_w, c->clip_y / c->clip_w,
               c->clip_z / c->clip_w);
    }
    return 0;
}

static int validate_uncompressed_us_rev1(void)
{
    u8 header[4];
    u8 dma_entry[16];
    u8 *segment = s_arwing_segment;
    u32 entry_offset = SF64_US_REV1_DMA_TABLE +
                       SF64_AST_ARWING_DMA_INDEX * 16U;
    u32 vrom_base;
    u32 segment_start;
    u32 segment_end;
    u32 flags;

    if (n64ps3_pi_read_rom(0, header, sizeof(header)) != 0 ||
        header[0] != 0x80U || header[1] != 0x37U ||
        header[2] != 0x12U || header[3] != 0x40U) {
        fprintf(stderr, "expected a big-endian N64 .z64 ROM\n");
        return -1;
    }

    if (n64ps3_pi_read_rom(entry_offset, dma_entry, sizeof(dma_entry)) != 0) {
        fprintf(stderr, "ROM is too short to contain the US Rev 1 DMA table\n");
        return -1;
    }

    vrom_base = read_be32(dma_entry);
    segment_start = read_be32(dma_entry + 4U);
    segment_end = read_be32(dma_entry + 8U);
    flags = read_be32(dma_entry + 12U);
    if (vrom_base != SF64_AST_ARWING_ROM_START ||
        segment_start != SF64_AST_ARWING_ROM_START ||
        segment_end != SF64_AST_ARWING_ROM_END || flags != 0U) {
        fprintf(stderr,
                "expected decompressed US Rev 1 ROM; ast_arwing DMA entry is "
                "[%08x,%08x,%08x,%08x]\n",
                vrom_base, segment_start, segment_end, flags);
        fprintf(stderr,
                "decompress baserom.us.rev1.z64 with sf64-psp/tools/comptool.py "
                "before running this probe\n");
        return -1;
    }

    if (n64ps3_pi_read_rom(SF64_AST_ARWING_ROM_START,
                           segment,
                           SF64_AST_ARWING_ROM_END - SF64_AST_ARWING_ROM_START) != 0) {
        fprintf(stderr, "could not read the ast_arwing bank from the ROM\n");
        return -1;
    }

    if (memcmp(segment + SF64_AST_ARWING_BODY_DL_OFFSET,
               s_expected_body_header, sizeof(s_expected_body_header)) != 0 ||
        memcmp(segment + SF64_AST_ARWING_SKELETON_OFFSET,
               s_expected_skeleton_root, sizeof(s_expected_skeleton_root)) != 0) {
        fprintf(stderr, "ast_arwing bank does not match US Rev 1 asset metadata\n");
        return -1;
    }

    printf("loaded US Rev 1 ast_arwing bank: ROM [%08x,%08x), %u bytes; "
           "segment %u\n",
           SF64_AST_ARWING_ROM_START, SF64_AST_ARWING_ROM_END,
           SF64_AST_ARWING_ROM_END - SF64_AST_ARWING_ROM_START,
           SF64_ARWING_SEGMENT_ID);
    printf("body display list: segment %u:%06x (ROM %08x)\n",
           SF64_ARWING_SEGMENT_ID, SF64_AST_ARWING_BODY_DL_OFFSET,
           SF64_AST_ARWING_BODY_DL_ROM_OFFSET);
    return 0;
}

static int load_extracted_segment(const char *path)
{
    FILE *input = fopen(path, "rb");
    size_t bytes_read;
    int trailing_byte;

    if (!input) {
        fprintf(stderr, "could not open extracted Arwing bank: %s\n", path);
        return -1;
    }
    bytes_read = fread(s_arwing_segment, 1U, sizeof(s_arwing_segment), input);
    trailing_byte = fgetc(input);
    if (fclose(input) != 0 || bytes_read != sizeof(s_arwing_segment) ||
        trailing_byte != EOF) {
        fprintf(stderr,
                "extracted Arwing bank must be exactly %u bytes: %s\n",
                (u32)sizeof(s_arwing_segment), path);
        return -1;
    }
    if (memcmp(s_arwing_segment + SF64_AST_ARWING_BODY_DL_OFFSET,
               s_expected_body_header, sizeof(s_expected_body_header)) != 0 ||
        memcmp(s_arwing_segment + SF64_AST_ARWING_SKELETON_OFFSET,
               s_expected_skeleton_root, sizeof(s_expected_skeleton_root)) != 0) {
        fprintf(stderr,
                "extracted bank does not match US Rev 1 Arwing metadata: %s\n",
                path);
        return -1;
    }

    printf("loaded extracted US Rev 1 ast_arwing bank: %u bytes; segment %u\n",
           (u32)sizeof(s_arwing_segment), SF64_ARWING_SEGMENT_ID);
    printf("body display list: segment %u:%06x\n",
           SF64_ARWING_SEGMENT_ID, SF64_AST_ARWING_BODY_DL_OFFSET);
    return 0;
}

int main(int argc, char **argv)
{
    N64ps3GfxTask task;
    N64ps3GfxTask next_task;
    N64ps3GfxF3dex1Stats stats;
    N64ps3GfxF3dex1Stats next_stats;
    N64ps3GfxF3dex1Status status;
    N64ps3GfxF3dex1Status next_status;
    Sf64ArwingBuildInfo build_info;
    Sf64ArwingBuildInfo next_build_info;
    Sf64ArwingProbe probe;
    Sf64ArwingProbe next_probe;
    u32 starfield_command_bytes;
    u32 title_star_count;
    u32 next_starfield_command_bytes;
    u32 next_title_star_count;
    u32 opcode;
    int rom_mounted = 0;

    if (argc == 2) {
        if (n64ps3_pi_mount_rom(argv[1]) != 0) {
            return 2;
        }
        rom_mounted = 1;
        if (validate_uncompressed_us_rev1() != 0) {
            n64ps3_pi_unmount_rom();
            return 2;
        }
    } else if (argc == 3 && strcmp(argv[1], "--segment") == 0) {
        if (load_extracted_segment(argv[2]) != 0) {
            return 2;
        }
    } else {
        fprintf(stderr,
                "usage: %s <uncompressed-us-rev1-rom.z64> | "
                "%s --segment <extracted-ast_arwing.bin>\n",
                argv[0], argv[0]);
        return 2;
    }

    memset(&probe, 0, sizeof(probe));
    if (sf64_title_starfield_build(s_title_starfield,
                                  (u32)sizeof(s_title_starfield),
                                  &starfield_command_bytes,
                                  &title_star_count) != 0 ||
        sf64_arwing_build_title_task_with_starfield(
            s_arwing_segment,
            (u32)sizeof(s_arwing_segment),
            s_title_starfield,
            starfield_command_bytes,
            title_star_count,
            s_rdram,
            (u32)sizeof(s_rdram),
            &task,
            &build_info) != 0) {
        if (rom_mounted) {
            n64ps3_pi_unmount_rom();
        }
        fprintf(stderr, "could not build title-pose Arwing task from the ROM bank\n");
        return 1;
    }
    if (sf64_title_starfield_build(s_next_title_starfield,
                                   (u32)sizeof(s_next_title_starfield),
                                   &next_starfield_command_bytes,
                                   &next_title_star_count) != 0 ||
        (starfield_command_bytes == next_starfield_command_bytes &&
         memcmp(s_title_starfield, s_next_title_starfield,
                starfield_command_bytes) == 0)) {
        if (rom_mounted) {
            n64ps3_pi_unmount_rom();
        }
        fprintf(stderr, "original title starfield did not advance between frames\n");
        return 1;
    }
    printf("original title starfield: stars=%u, commands=%u\n",
           title_star_count, starfield_command_bytes / 8U);
    printf("next original title frame: stars=%u, commands=%u (scroll advanced)\n",
           next_title_star_count, next_starfield_command_bytes / 8U);
    printf("title pose: animation frame %u, skeleton limbs=%u, display lists=%u, "
           "stars=%u, root commands=%u\n",
           build_info.animation_frame, build_info.skeleton_limb_count,
           build_info.display_list_count, build_info.title_star_count,
           build_info.generated_command_count);

    status = n64ps3_gfx_decode_f3dex1(&task, record_triangle, &probe, &stats);
    if (rom_mounted) {
        n64ps3_pi_unmount_rom();
    }

    printf("F3DEX1 status=%d commands=%u vertex_loads=%u triangles=%u "
           "nested_lists=%u matrices=%u textures=%u textured_triangles=%u "
           "fill_colors=%u fill_rectangles=%u skipped=%u\n",
           (int)status, stats.command_count, stats.vertex_load_count,
           stats.triangle_count, stats.display_list_call_count,
           stats.matrix_command_count, stats.texture_load_count,
           stats.textured_triangle_count, stats.fill_color_command_count,
           stats.fill_rectangle_count, stats.unsupported_command_count);
    printf("unique RGBA16 texture sources=%u\n", probe.unique_textures);
    printf("first textured triangle UV (N64 10.5): (%d,%d),(%d,%d),(%d,%d)\n",
           probe.first_textured_st[0], probe.first_textured_st[1],
           probe.first_textured_st[2], probe.first_textured_st[3],
           probe.first_textured_st[4], probe.first_textured_st[5]);
    printf("first textured tile: origin=(%u,%u) mask=(%u,%u) shift=(%u,%u) wrap=(%u,%u); "
           "nonzero origins=%u shifts=%u\n",
           probe.first_texture_uls, probe.first_texture_ult,
           probe.first_texture_mask_s, probe.first_texture_mask_t,
           probe.first_texture_shift_s, probe.first_texture_shift_t,
           probe.first_texture_wrap_s, probe.first_texture_wrap_t,
           probe.nonzero_tile_origins, probe.nonzero_tile_shifts);
    for (opcode = 0; opcode < probe.unique_textures; opcode++) {
        printf("  texture %u: %ux%u\n", opcode,
               probe.texture_widths[opcode], probe.texture_heights[opcode]);
    }
    for (opcode = 0; opcode < 256U; opcode++) {
        if (stats.unsupported_opcode_counts[opcode] != 0U) {
            printf("unsupported opcode 0x%02x (%s): %u\n",
                   opcode, opcode_name(opcode),
                   stats.unsupported_opcode_counts[opcode]);
        }
    }
    printf("NDC bounds x=[%.3f,%.3f] y=[%.3f,%.3f] z=[%.3f,%.3f], "
           "vertices inside viewport=%u\n",
           probe.minimum[0], probe.maximum[0],
           probe.minimum[1], probe.maximum[1],
           probe.minimum[2], probe.maximum[2], probe.vertices_in_clip);

    memset(&next_probe, 0, sizeof(next_probe));
    if (sf64_arwing_build_title_task_with_starfield(
            s_arwing_segment,
            (u32)sizeof(s_arwing_segment),
            s_next_title_starfield,
            next_starfield_command_bytes,
            next_title_star_count,
            s_rdram,
            (u32)sizeof(s_rdram),
            &next_task,
            &next_build_info) != 0) {
        fprintf(stderr, "could not build the next original title frame\n");
        return 1;
    }
    next_status = n64ps3_gfx_decode_f3dex1(&next_task, record_triangle,
                                           &next_probe, &next_stats);
    printf("next F3DEX1 status=%d commands=%u triangles=%u stars=%u "
           "textured_triangles=%u skipped=%u\n",
           (int)next_status, next_stats.command_count,
           next_stats.triangle_count, next_build_info.title_star_count,
           next_stats.textured_triangle_count,
           next_stats.unsupported_command_count);

    {
        u32 texture_index;
        int same_textures = probe.unique_textures == next_probe.unique_textures;
        int same_uv = memcmp(probe.first_textured_st,
                             next_probe.first_textured_st,
                             sizeof(probe.first_textured_st)) == 0;
        int same_texture_state = probe.texture_state_hash ==
                                 next_probe.texture_state_hash;
        int same_shading = probe.texture_shade_hash ==
                           next_probe.texture_shade_hash;
        u32 max_shade_delta = 0U;

        for (texture_index = 0;
            same_textures && texture_index < probe.unique_textures;
            texture_index++) {
            if (probe.texture_widths[texture_index] !=
                    next_probe.texture_widths[texture_index] ||
                probe.texture_heights[texture_index] !=
                    next_probe.texture_heights[texture_index] ||
                probe.texture_hashes[texture_index] !=
                    next_probe.texture_hashes[texture_index]) {
                same_textures = 0;
            }
        }
        for (texture_index = 0;
             texture_index < probe.textured_triangles &&
             texture_index < next_probe.textured_triangles &&
             texture_index < SF64_ARWING_PROBE_MAX_TEXTURED_TRIANGLES;
             texture_index++) {
            u32 vertex_index;
            for (vertex_index = 0; vertex_index < 3U; vertex_index++) {
                u32 channel;
                for (channel = 0; channel < 4U; channel++) {
                    u8 first = probe.texture_shade[texture_index]
                                                   [vertex_index][channel];
                    u8 next = next_probe.texture_shade[texture_index]
                                                       [vertex_index][channel];
                    u32 delta = first > next ? first - next : next - first;
                    if (delta > max_shade_delta) {
                        max_shade_delta = delta;
                    }
                }
            }
        }
        printf("two-frame texture-source hashes=%s; first-triangle UVs=%s; "
               "all texture-coordinate state=%s (%08x/%08x); "
               "vertex shade=%s (%08x/%08x), max component delta=%u\n",
               same_textures ? "identical" : "different",
               same_uv ? "identical" : "different",
               same_texture_state ? "identical" : "different",
               probe.texture_state_hash, next_probe.texture_state_hash,
               same_shading ? "identical" : "different",
               probe.texture_shade_hash, next_probe.texture_shade_hash,
               max_shade_delta);
    }

    if (status != N64PS3_GFX_F3DEX1_OK ||
        build_info.title_star_count != 456U ||
        stats.fill_color_command_count != build_info.title_star_count ||
        stats.fill_rectangle_count != build_info.title_star_count ||
        stats.triangle_count != 174U + 2U * build_info.title_star_count ||
        stats.textured_triangle_count != 174U ||
        stats.unsupported_command_count != 0U ||
        probe.triangles != stats.triangle_count ||
        next_status != N64PS3_GFX_F3DEX1_OK ||
        next_build_info.title_star_count != next_title_star_count ||
        next_stats.fill_rectangle_count != next_title_star_count ||
        next_stats.textured_triangle_count != 174U ||
        next_stats.unsupported_command_count != 0U ||
        memcmp(probe.first_textured_clip, next_probe.first_textured_clip,
               sizeof(probe.first_textured_clip)) == 0) {
        fprintf(stderr,
                "n64ps3 did not decode the original starfield and Arwing lists\n");
        return 1;
    }
    return 0;
}

#include "n64ps3/gfx.h"
#include "n64ps3/gfx_f3dex1.h"
#include "n64ps3/pi.h"
#include "sf64_ps3/arwing.h"

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

static const u8 s_expected_body_header[] = {
    0xfd, 0x10, 0x00, 0x00, 0x03, 0x00, 0xac, 0x48
};

static const u8 s_expected_skeleton_root[] = {
    0x03, 0x01, 0x65, 0xf0
};

static u8 s_arwing_segment[SF64_AST_ARWING_SEGMENT_BYTES];
static u8 s_rdram[SF64_ARWING_RDRAM_REQUIRED_SIZE];

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
    const u8 *texture_sources[64];
    u16 texture_widths[64];
    u16 texture_heights[64];
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
    u32 axis;

    probe->triangles++;
    if (a->texture_enabled && a->texture_rgba16) {
        u32 texture_index;
        probe->textured_triangles++;
        for (texture_index = 0; texture_index < probe->unique_textures;
             texture_index++) {
            if (probe->texture_sources[texture_index] == a->texture_rgba16 &&
                probe->texture_widths[texture_index] == a->texture_width &&
                probe->texture_heights[texture_index] == a->texture_height) {
                break;
            }
        }
        if (texture_index == probe->unique_textures &&
            probe->unique_textures < 64U) {
            probe->texture_sources[texture_index] = a->texture_rgba16;
            probe->texture_widths[texture_index] = a->texture_width;
            probe->texture_heights[texture_index] = a->texture_height;
            probe->unique_textures++;
        }
    }
    for (vertex_index = 0; vertex_index < 3U; vertex_index++) {
        const float coordinates[3] = {
            vertices[vertex_index]->clip_x,
            vertices[vertex_index]->clip_y,
            vertices[vertex_index]->clip_z
        };
        if (coordinates[0] >= -1.0f && coordinates[0] <= 1.0f &&
            coordinates[1] >= -1.0f && coordinates[1] <= 1.0f) {
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
               "clip=(%.3f,%.3f,%.3f),(%.3f,%.3f,%.3f),(%.3f,%.3f,%.3f)\n",
               probe->triangles,
               a->x, a->y, a->z, b->x, b->y, b->z, c->x, c->y, c->z,
               a->clip_x, a->clip_y, a->clip_z,
               b->clip_x, b->clip_y, b->clip_z,
               c->clip_x, c->clip_y, c->clip_z);
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

int main(int argc, char **argv)
{
    N64ps3GfxTask task;
    N64ps3GfxF3dex1Stats stats;
    N64ps3GfxF3dex1Status status;
    Sf64ArwingBuildInfo build_info;
    Sf64ArwingProbe probe;
    u32 opcode;

    if (argc != 2) {
        fprintf(stderr,
                "usage: %s <uncompressed-us-rev1-rom.z64>\n",
                argv[0]);
        return 2;
    }
    if (n64ps3_pi_mount_rom(argv[1]) != 0) {
        return 2;
    }

    if (validate_uncompressed_us_rev1() != 0) {
        n64ps3_pi_unmount_rom();
        return 2;
    }

    memset(&probe, 0, sizeof(probe));
    if (sf64_arwing_build_title_task(s_arwing_segment,
                                     (u32)sizeof(s_arwing_segment),
                                     s_rdram,
                                     (u32)sizeof(s_rdram),
                                     &task,
                                     &build_info) != 0) {
        n64ps3_pi_unmount_rom();
        fprintf(stderr, "could not build title-pose Arwing task from the ROM bank\n");
        return 1;
    }
    printf("title pose: animation frame %u, skeleton limbs=%u, display lists=%u, "
           "root commands=%u\n",
           build_info.animation_frame, build_info.skeleton_limb_count,
           build_info.display_list_count, build_info.generated_command_count);

    status = n64ps3_gfx_decode_f3dex1(&task, record_triangle, &probe, &stats);
    n64ps3_pi_unmount_rom();

    printf("F3DEX1 status=%d commands=%u vertex_loads=%u triangles=%u "
           "nested_lists=%u matrices=%u textures=%u textured_triangles=%u "
           "skipped=%u\n",
           (int)status, stats.command_count, stats.vertex_load_count,
           stats.triangle_count, stats.display_list_call_count,
           stats.matrix_command_count, stats.texture_load_count,
           stats.textured_triangle_count, stats.unsupported_command_count);
    printf("unique RGBA16 texture sources=%u\n", probe.unique_textures);
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
    printf("clip bounds x=[%.3f,%.3f] y=[%.3f,%.3f] z=[%.3f,%.3f], "
           "vertices inside viewport=%u\n",
           probe.minimum[0], probe.maximum[0],
           probe.minimum[1], probe.maximum[1],
           probe.minimum[2], probe.maximum[2], probe.vertices_in_clip);

    if (status != N64PS3_GFX_F3DEX1_OK ||
        stats.triangle_count == 0U || probe.triangles != stats.triangle_count) {
        fprintf(stderr, "n64ps3 did not decode original Arwing body geometry\n");
        return 1;
    }
    return 0;
}

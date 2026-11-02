#include "n64ps3/gfx_f3dex1.h"

#include <stdio.h>
#include <string.h>

#define OPTION_SIZE 0x16090U
#define OPTION_BASE 0x1000U
#define CARD_TEXTURE_OFFSET 0x155e8U

static unsigned char s_memory[OPTION_BASE + OPTION_SIZE];
static const unsigned char s_root[] = {
    0xbc, 0x00, 0x20, 0x06, 0x80, 0x00, 0x10, 0x00,
    0xd7, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff,
    0x06, 0x00, 0x00, 0x00, 0x08, 0x01, 0x55, 0x50,
    0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static int check_triangle(void *context, const N64ps3GfxF3dex1Vertex *a,
                          const N64ps3GfxF3dex1Vertex *b,
                          const N64ps3GfxF3dex1Vertex *c)
{
    unsigned *count = context;
    const unsigned char *expected = s_memory + OPTION_BASE + CARD_TEXTURE_OFFSET;

    if (a->texture_data != expected || b->texture_data != expected ||
        c->texture_data != expected || a->texture_width != 32 ||
        a->texture_height != 32 || !a->texture_enabled) {
        fprintf(stderr, "option card triangle has unexpected texture state\n");
        return -1;
    }
    (*count)++;
    return 0;
}

int main(int argc, char **argv)
{
    static const struct {
        const char *name;
        unsigned offset;
        unsigned width;
        unsigned height;
    } labels[] = {
        { "main game", 0x3b50U, 72U, 12U },
        { "training", 0x3eb0U, 80U, 12U },
        { "versus", 0x4270U, 32U, 10U },
        { "ranking", 0x43b0U, 64U, 12U },
        { "sound", 0x46b0U, 64U, 10U },
        { "data", 0x4930U, 80U, 10U },
    };
    N64ps3GfxTask task;
    N64ps3GfxF3dex1Stats stats;
    N64ps3GfxF3dex1Status status;
    unsigned triangles = 0;
    FILE *input;
    size_t bytes;
    int extra;
    unsigned index;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <ast_option.bin>\n", argv[0]);
        return 2;
    }
    input = fopen(argv[1], "rb");
    if (!input) {
        perror(argv[1]);
        return 1;
    }
    bytes = fread(s_memory + OPTION_BASE, 1, OPTION_SIZE, input);
    extra = fgetc(input);
    if (fclose(input) != 0 || bytes != OPTION_SIZE || extra != EOF) {
        fprintf(stderr, "option bank has an unexpected size\n");
        return 1;
    }
    for (index = 0; index < sizeof(labels) / sizeof(labels[0]); index++) {
        unsigned texel;
        unsigned visible = 0;
        unsigned size = labels[index].width * labels[index].height;
        if (labels[index].offset > OPTION_SIZE || size > OPTION_SIZE - labels[index].offset) {
            fprintf(stderr, "option label extends beyond its bank\n");
            return 1;
        }
        for (texel = 0; texel < size; texel++) {
            if (s_memory[OPTION_BASE + labels[index].offset + texel] & 0x0fU) visible++;
        }
        if (!visible) {
            fprintf(stderr, "option label %s has no visible pixels\n", labels[index].name);
            return 1;
        }
        printf("option label %s: %ux%u, %u visible texels\n", labels[index].name,
               labels[index].width, labels[index].height, visible);
    }
    memset(&task, 0, sizeof(task));
    task.kind = N64PS3_GFX_TASK_DISPLAY_LIST;
    task.dram_base = s_memory;
    task.dram_size_bytes = sizeof(s_memory);
    task.display_list = s_root;
    task.display_list_size_bytes = sizeof(s_root);
    status = n64ps3_gfx_decode_f3dex1(&task, check_triangle, &triangles, &stats);
    printf("option card: status=%d commands=%u triangles=%u textures=%u skipped=%u\n",
           status, stats.command_count, stats.triangle_count,
           stats.texture_load_count, stats.unsupported_command_count);
    return status == N64PS3_GFX_F3DEX1_OK && triangles == 2 &&
           stats.triangle_count == 2 && stats.texture_load_count == 1 &&
           stats.unsupported_command_count == 0 ? 0 : 1;
}
